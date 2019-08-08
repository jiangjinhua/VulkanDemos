#include "Common/Common.h"
#include "Common/Log.h"

#include "Demo/DVKCommon.h"
#include "Demo/DVKTexture.h"
#include "Demo/DVKRenderTarget.h"

#include "Math/Vector4.h"
#include "Math/Matrix4x4.h"
#include "Math/Quat.h"

#include "Loader/ImageLoader.h"
#include "File/FileManager.h"
#include "UI/ImageGUIContext.h"

#include <vector>
#include <fstream>

class SkeletonMatrix4x4Demo : public DemoBase
{
public:
	SkeletonMatrix4x4Demo(int32 width, int32 height, const char* title, const std::vector<std::string>& cmdLine)
		: DemoBase(width, height, title, cmdLine)
	{

	}

	virtual ~SkeletonMatrix4x4Demo()
	{

	}

	virtual bool PreInit() override
	{
		return true;
	}

	virtual bool Init() override
	{
		DemoBase::Setup();
		DemoBase::Prepare();
        
        LoadAssets();
		InitParmas();
		CreateGUI();
        
		m_Ready = true;

		return true;
	}

	virtual void Exist() override
	{
		DemoBase::Release();
		DestroyAssets();
		DestroyGUI();
	}

	virtual void Loop(float time, float delta) override
	{
		if (!m_Ready) {
			return;
		}
		Draw(time, delta);
	}

private:

	struct ModelViewProjectionBlock
	{
		Matrix4x4 model;
		Matrix4x4 view;
		Matrix4x4 projection;
	};

#define MAX_BONES 64
	struct BonesTransformBlock
	{
		Matrix4x4 bones[MAX_BONES];
	};
    
	void Draw(float time, float delta)
	{
		int32 bufferIndex = DemoBase::AcquireBackbufferIndex();
        
		UpdateUI(time, delta);
        
		UpdateAnimation(time, delta);
        
		// 设置Room参数
        // m_RoleModel->rootNode->localMatrix.AppendRotation(delta * 90.0f, Vector3::UpVector);
        m_RoleMaterial->BeginFrame();
        for (int32 i = 0; i < m_RoleModel->meshes.size(); ++i)
        {
			vk_demo::DVKMesh* mesh = m_RoleModel->meshes[i];
            
			// model data
            m_MVPData.model = mesh->linkNode->GetGlobalMatrix();
            
			// bones data
			for (int32 j = 0; j < mesh->bones.size(); ++j) {
				vk_demo::DVKBone& bone = mesh->bones[j];
				std::string& boneName  = bone.name;
				vk_demo::DVKNode* node = m_RoleModel->nodesMap[boneName];
                // 注意行列顺序，Vertex * inverseBindPose * globalBone
				m_BonesData.bones[j] = bone.inverseBindPose;
				m_BonesData.bones[j].Append(node->GetGlobalMatrix());
                // 这里要注意，我们的Bone动画使用的是全局变化矩阵，变换矩阵一直延续到了aiScene->mRoot节点。
                // 因此我们需要将Bone变换矩阵与mesh的全局变化矩阵的逆矩阵做运算，来抵消掉mesh父节点之上的变换操作。
				m_BonesData.bones[j].Append(mesh->linkNode->GetGlobalMatrix().Inverse());
			}
            
            if (mesh->bones.size() == 0) {
                m_BonesData.bones[0].SetIdentity();
            }
            
			m_RoleMaterial->BeginObject();
			m_RoleMaterial->SetLocalUniform("bonesData", &m_BonesData, sizeof(BonesTransformBlock));
            m_RoleMaterial->SetLocalUniform("uboMVP",    &m_MVPData,   sizeof(ModelViewProjectionBlock));
            m_RoleMaterial->EndObject();
        }
        m_RoleMaterial->EndFrame();
        
		SetupCommandBuffers(bufferIndex);
        
		DemoBase::Present(bufferIndex);
	}

	void UpdateAnimation(float time, float delta)
	{
        if (m_AutoAnimation) {
            m_RoleModel->Update(time, delta);
        }
        else {
            m_RoleModel->GotoAnimation(m_AnimTime);
        }
	}
    
	void UpdateUI(float time, float delta)
	{
		m_GUI->StartFrame();

		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
			ImGui::Begin("SkeletonMatrix4x4Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            
            if (ImGui::SliderInt("Anim", &m_AnimIndex, 0, m_RoleModel->animations.size() - 1))
            {
                SetAnimation(m_AnimIndex);
            }
            
            ImGui::Checkbox("AutoPlay", &m_AutoAnimation);
            
            if (!m_AutoAnimation)
            {
                ImGui::SliderFloat("Time", &m_AnimTime, 0.0f, m_AnimDuration);
            }
            
			ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		m_GUI->EndFrame();
		m_GUI->Update();
	}
    
    void SetAnimation(int32 index)
    {
        m_RoleModel->SetAnimation(index);
        m_AnimDuration = m_RoleModel->animations[index].duration;
        m_AnimTime     = 0.0f;
        m_AnimIndex    = index;
    }
    
	void LoadAssets()
	{
		vk_demo::DVKCommandBuffer* cmdBuffer = vk_demo::DVKCommandBuffer::Create(m_VulkanDevice, m_CommandPool);
        
		// model
		m_RoleModel = vk_demo::DVKModel::LoadFromFile(
			"assets/models/xiaonan/nvhai.fbx",
			m_VulkanDevice,
			cmdBuffer,
			{
                VertexAttribute::VA_Position,
                VertexAttribute::VA_UV0,
                VertexAttribute::VA_Normal,
                VertexAttribute::VA_SkinIndex,
                VertexAttribute::VA_SkinWeight
            }
		);
        
        SetAnimation(0);
        
		// shader
		m_RoleShader = vk_demo::DVKShader::Create(
			m_VulkanDevice,
			true,
			"assets/shaders/26_SkeletonMatrix4x4/obj.vert.spv",
			"assets/shaders/26_SkeletonMatrix4x4/obj.frag.spv"
		);
        
        // texture
        m_RoleDiffuse = vk_demo::DVKTexture::Create2D(
            "assets/models/xiaonan/b001.jpg",
            m_VulkanDevice,
            cmdBuffer
        );
        
		// material
        m_RoleMaterial = vk_demo::DVKMaterial::Create(
            m_VulkanDevice,
            m_RenderPass,
            m_PipelineCache,
            m_RoleShader
        );
        m_RoleMaterial->PreparePipeline();
        m_RoleMaterial->SetTexture("diffuseMap", m_RoleDiffuse);
        
		delete cmdBuffer;
	}

	void DestroyAssets()
	{
		delete m_RoleShader;
        delete m_RoleDiffuse;
        delete m_RoleMaterial;
		delete m_RoleModel;
	}

	void SetupCommandBuffers(int32 backBufferIndex)
	{
		VkViewport viewport = {};
		viewport.x        = 0;
		viewport.y        = m_FrameHeight;
		viewport.width    = m_FrameWidth;
		viewport.height   = -(float)m_FrameHeight;    // flip y axis
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.extent.width  = m_FrameWidth;
		scissor.extent.height = m_FrameHeight;
		scissor.offset.x = 0;
		scissor.offset.y = 0;

		VkCommandBuffer commandBuffer = m_CommandBuffers[backBufferIndex];

		VkCommandBufferBeginInfo cmdBeginInfo;
		ZeroVulkanStruct(cmdBeginInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
		VERIFYVULKANRESULT(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));
        
        VkClearValue clearValues[2];
        clearValues[0].color        = { { 0.2f, 0.2f, 0.2f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };
        
        VkRenderPassBeginInfo renderPassBeginInfo;
        ZeroVulkanStruct(renderPassBeginInfo, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
        renderPassBeginInfo.renderPass               = m_RenderPass;
        renderPassBeginInfo.framebuffer              = m_FrameBuffers[backBufferIndex];
        renderPassBeginInfo.clearValueCount          = 2;
        renderPassBeginInfo.pClearValues             = clearValues;
        renderPassBeginInfo.renderArea.offset.x      = 0;
        renderPassBeginInfo.renderArea.offset.y      = 0;
        renderPassBeginInfo.renderArea.extent.width  = m_FrameWidth;
        renderPassBeginInfo.renderArea.extent.height = m_FrameHeight;
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer,  0, 1, &scissor);
        
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_RoleMaterial->GetPipeline());
        for (int32 j = 0; j < m_RoleModel->meshes.size(); ++j) {
            m_RoleMaterial->BindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, j);
            m_RoleModel->meshes[j]->BindDrawCmd(commandBuffer);
        }
        
        m_GUI->BindDrawCmd(commandBuffer, m_RenderPass);
        
        vkCmdEndRenderPass(commandBuffer);

		VERIFYVULKANRESULT(vkEndCommandBuffer(commandBuffer));
	}

	void InitParmas()
	{
        vk_demo::DVKBoundingBox bounds = m_RoleModel->rootNode->GetBounds();
        Vector3 boundSize   = bounds.max - bounds.min;
        Vector3 boundCenter = bounds.min + boundSize * 0.5f;
        boundCenter.z -= boundSize.Size() * 1.5f;
        boundCenter.y += 10;
        
		m_MVPData.model.SetIdentity();
        
		m_MVPData.view.SetIdentity();
		m_MVPData.view.SetOrigin(boundCenter);
		m_MVPData.view.SetInverse();

		m_MVPData.projection.SetIdentity();
		m_MVPData.projection.Perspective(MMath::DegreesToRadians(75.0f), (float)GetWidth(), (float)GetHeight(), 10.0f, 3000.0f);
        
        for (int32 i = 0; i < MAX_BONES; ++i) {
            m_BonesData.bones[i].SetIdentity();
        }
	}
    
	void CreateGUI()
	{
		m_GUI = new ImageGUIContext();
		m_GUI->Init("assets/fonts/Roboto-Medium.ttf");
	}

	void DestroyGUI()
	{
		m_GUI->Destroy();
		delete m_GUI;
	}

private:
    
	bool 						m_Ready = false;
    
	ModelViewProjectionBlock	m_MVPData;
	BonesTransformBlock			m_BonesData;

	vk_demo::DVKModel*			m_RoleModel = nullptr;
	vk_demo::DVKShader*			m_RoleShader = nullptr;
	vk_demo::DVKTexture*		m_RoleDiffuse = nullptr;
    vk_demo::DVKMaterial*       m_RoleMaterial = nullptr;
    
	ImageGUIContext*			m_GUI = nullptr;
    
    bool                        m_AutoAnimation = false;
    float                       m_AnimDuration = 0.0f;
    float                       m_AnimTime = 0.0f;
    int32                       m_AnimIndex = 0;
};

std::shared_ptr<AppModuleBase> CreateAppMode(const std::vector<std::string>& cmdLine)
{
	return std::make_shared<SkeletonMatrix4x4Demo>(1400, 900, "SkeletonMatrix4x4Demo", cmdLine);
}
