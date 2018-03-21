// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "device/vr/openvr/test/fake_openvr_log.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "third_party/openvr/src/src/ivrclientcore.h"

#include <D3D11_1.h>
#include <DXGI1_4.h>
#include <wrl.h>
#include <memory>

namespace vr {

class TestVRLogger {
 public:
  void Start() {
    // Look for environment variable saying we should log data.
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    std::string log_filename;
    if (env->GetVar(GetVrPixelLogEnvVarName(), &log_filename)) {
      base::ScopedAllowBlockingForTesting allow_files;
      log_file_ = std::make_unique<base::File>(
          base::FilePath::FromUTF8Unsafe(log_filename),
          base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
              base::File::FLAG_EXCLUSIVE_WRITE);
      logging_ = log_file_->IsValid();
    }
  }

  void Stop() {
    base::ScopedAllowBlockingForTesting allow_files;
    if (logging_) {
      log_file_->Flush();
      log_file_->Close();
      log_file_ = nullptr;
    }
    logging_ = false;
  }

  void OnPresentedFrame(ID3D11Texture2D* texture) {
    if (!logging_)
      return;

    VRSubmittedFrameEvent frame;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    texture->GetDevice(&device);

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    // We copy the submitted texture to a new texture, so we can map it, and
    // read back pixel data.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_copy;
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    desc.Width = 1;
    desc.Height = 1;
    desc.MiscFlags = 0;
    desc.BindFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture_copy);
    if (FAILED(hr)) {
      // We'll have invalid data in log (no frame data) so test will fail.
      Stop();
      return;
    }

    D3D11_BOX box = {0, 0, 0, 1, 1, 1};  // a 1-pixel box
    context->CopySubresourceRegion(texture_copy.Get(), 0, 0, 0, 0, texture, 0,
                                   &box);

    D3D11_MAPPED_SUBRESOURCE map_data = {};
    hr = context->Map(texture_copy.Get(), 0, D3D11_MAP_READ, 0, &map_data);
    if (FAILED(hr)) {
      // We'll have invalid data in log (no frame data) so test will fail.
      Stop();
      return;
    }

    VRSubmittedFrameEvent::Color* data =
        reinterpret_cast<VRSubmittedFrameEvent::Color*>(map_data.pData);
    frame.color = data[0];  // Save top-left pixel value.
    context->Unmap(texture_copy.Get(), 0);

    WriteVRSubmittedFrameEvent(&frame);
    Stop();  // For now only validate one pixel from first frame.
  }

 private:
  void WriteVRSubmittedFrameEvent(VRSubmittedFrameEvent* frame) {
    base::ScopedAllowBlockingForTesting allow_files;
    log_file_->WriteAtCurrentPos(reinterpret_cast<char*>(frame),
                                 sizeof(*frame));
  }

  bool logging_ = false;
  std::unique_ptr<base::File> log_file_;
};

class TestVRSystem : public IVRSystem {
 public:
  void GetRecommendedRenderTargetSize(uint32_t* pnWidth,
                                      uint32_t* pnHeight) override;
  HmdMatrix44_t GetProjectionMatrix(EVREye eEye,
                                    float fNearZ,
                                    float fFarZ) override {
    NOTIMPLEMENTED();
    return {};
  };
  void GetProjectionRaw(EVREye eEye,
                        float* pfLeft,
                        float* pfRight,
                        float* pfTop,
                        float* pfBottom) override;
  bool ComputeDistortion(
      EVREye eEye,
      float fU,
      float fV,
      DistortionCoordinates_t* pDistortionCoordinates) override {
    NOTIMPLEMENTED();
    return false;
  }
  HmdMatrix34_t GetEyeToHeadTransform(EVREye eEye) override;
  bool GetTimeSinceLastVsync(float* pfSecondsSinceLastVsync,
                             uint64_t* pulFrameCounter) override {
    NOTIMPLEMENTED();
    return false;
  }
  int32_t GetD3D9AdapterIndex() override {
    NOTIMPLEMENTED();
    return 0;
  }
  void GetDXGIOutputInfo(int32_t* pnAdapterIndex) override;
  bool IsDisplayOnDesktop() override {
    NOTIMPLEMENTED();
    return false;
  }
  bool SetDisplayVisibility(bool bIsVisibleOnDesktop) override {
    NOTIMPLEMENTED();
    return false;
  }
  void GetDeviceToAbsoluteTrackingPose(
      ETrackingUniverseOrigin eOrigin,
      float fPredictedSecondsToPhotonsFromNow,
      VR_ARRAY_COUNT(unTrackedDevicePoseArrayCount)
          TrackedDevicePose_t* pTrackedDevicePoseArray,
      uint32_t unTrackedDevicePoseArrayCount) override;
  void ResetSeatedZeroPose() override { NOTIMPLEMENTED(); }
  HmdMatrix34_t GetSeatedZeroPoseToStandingAbsoluteTrackingPose() override;
  HmdMatrix34_t GetRawZeroPoseToStandingAbsoluteTrackingPose() override {
    NOTIMPLEMENTED();
    return {};
  }
  uint32_t GetSortedTrackedDeviceIndicesOfClass(
      ETrackedDeviceClass eTrackedDeviceClass,
      VR_ARRAY_COUNT(unTrackedDeviceIndexArrayCount)
          TrackedDeviceIndex_t* punTrackedDeviceIndexArray,
      uint32_t unTrackedDeviceIndexArrayCount,
      TrackedDeviceIndex_t unRelativeToTrackedDeviceIndex =
          k_unTrackedDeviceIndex_Hmd) override {
    NOTIMPLEMENTED();
    return 0;
  }
  EDeviceActivityLevel GetTrackedDeviceActivityLevel(
      TrackedDeviceIndex_t unDeviceId) override {
    NOTIMPLEMENTED();
    return k_EDeviceActivityLevel_Unknown;
  }
  void ApplyTransform(TrackedDevicePose_t* pOutputPose,
                      const TrackedDevicePose_t* pTrackedDevicePose,
                      const HmdMatrix34_t* pTransform) override {
    NOTIMPLEMENTED();
  }
  TrackedDeviceIndex_t GetTrackedDeviceIndexForControllerRole(
      ETrackedControllerRole unDeviceType) override {
    NOTIMPLEMENTED();
    return 0;
  }
  ETrackedControllerRole GetControllerRoleForTrackedDeviceIndex(
      TrackedDeviceIndex_t unDeviceIndex) override {
    NOTIMPLEMENTED();
    return TrackedControllerRole_Invalid;
  }
  ETrackedDeviceClass GetTrackedDeviceClass(
      TrackedDeviceIndex_t unDeviceIndex) override {
    NOTIMPLEMENTED();
    return TrackedDeviceClass_Invalid;
  }
  bool IsTrackedDeviceConnected(TrackedDeviceIndex_t unDeviceIndex) override {
    NOTIMPLEMENTED();
    return false;
  }
  bool GetBoolTrackedDeviceProperty(
      TrackedDeviceIndex_t unDeviceIndex,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* pError = 0L) override {
    NOTIMPLEMENTED();
    return false;
  }
  float GetFloatTrackedDeviceProperty(
      TrackedDeviceIndex_t unDeviceIndex,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* pError = 0L) override;
  int32_t GetInt32TrackedDeviceProperty(
      TrackedDeviceIndex_t unDeviceIndex,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* pError = 0L) override {
    NOTIMPLEMENTED();
    return 0;
  }
  uint64_t GetUint64TrackedDeviceProperty(
      TrackedDeviceIndex_t unDeviceIndex,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* pError = 0L) override {
    NOTIMPLEMENTED();
    return 0;
  }
  HmdMatrix34_t GetMatrix34TrackedDeviceProperty(
      TrackedDeviceIndex_t unDeviceIndex,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* pError = 0L) override {
    NOTIMPLEMENTED();
    return {};
  }
  uint32_t GetStringTrackedDeviceProperty(
      TrackedDeviceIndex_t unDeviceIndex,
      ETrackedDeviceProperty prop,
      VR_OUT_STRING() char* pchValue,
      uint32_t unBufferSize,
      ETrackedPropertyError* pError = 0L) override;
  const char* GetPropErrorNameFromEnum(ETrackedPropertyError error) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  bool PollNextEvent(VREvent_t* pEvent, uint32_t uncbVREvent) override;
  bool PollNextEventWithPose(ETrackingUniverseOrigin eOrigin,
                             VREvent_t* pEvent,
                             uint32_t uncbVREvent,
                             TrackedDevicePose_t* pTrackedDevicePose) override {
    NOTIMPLEMENTED();
    return false;
  }
  const char* GetEventTypeNameFromEnum(EVREventType eType) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  HiddenAreaMesh_t GetHiddenAreaMesh(
      EVREye eEye,
      EHiddenAreaMeshType type = k_eHiddenAreaMesh_Standard) override {
    NOTIMPLEMENTED();
    return {};
  }
  bool GetControllerState(TrackedDeviceIndex_t unControllerDeviceIndex,
                          VRControllerState_t* pControllerState,
                          uint32_t unControllerStateSize) override {
    NOTIMPLEMENTED();
    return false;
  }
  bool GetControllerStateWithPose(
      ETrackingUniverseOrigin eOrigin,
      TrackedDeviceIndex_t unControllerDeviceIndex,
      VRControllerState_t* pControllerState,
      uint32_t unControllerStateSize,
      TrackedDevicePose_t* pTrackedDevicePose) override {
    NOTIMPLEMENTED();
    return false;
  }
  void TriggerHapticPulse(TrackedDeviceIndex_t unControllerDeviceIndex,
                          uint32_t unAxisId,
                          unsigned short usDurationMicroSec) override {
    NOTIMPLEMENTED();
  }
  const char* GetButtonIdNameFromEnum(EVRButtonId eButtonId) override {
    NOTIMPLEMENTED();
    return nullptr;
  };
  const char* GetControllerAxisTypeNameFromEnum(
      EVRControllerAxisType eAxisType) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  bool CaptureInputFocus() override {
    NOTIMPLEMENTED();
    return false;
  }
  void ReleaseInputFocus() override { NOTIMPLEMENTED(); }
  bool IsInputFocusCapturedByAnotherProcess() override {
    NOTIMPLEMENTED();
    return false;
  }
  uint32_t DriverDebugRequest(TrackedDeviceIndex_t unDeviceIndex,
                              const char* pchRequest,
                              char* pchResponseBuffer,
                              uint32_t unResponseBufferSize) override {
    NOTIMPLEMENTED();
    return 0;
  }
  EVRFirmwareError PerformFirmwareUpdate(
      TrackedDeviceIndex_t unDeviceIndex) override {
    NOTIMPLEMENTED();
    return VRFirmwareError_None;
  }
  void AcknowledgeQuit_Exiting() override { NOTIMPLEMENTED(); }
  void AcknowledgeQuit_UserPrompt() override { NOTIMPLEMENTED(); }
};

class TestVRCompositor : public IVRCompositor {
 public:
  void SetTrackingSpace(ETrackingUniverseOrigin eOrigin) override;
  ETrackingUniverseOrigin GetTrackingSpace() override {
    NOTIMPLEMENTED();
    return TrackingUniverseSeated;
  }
  EVRCompositorError WaitGetPoses(VR_ARRAY_COUNT(unRenderPoseArrayCount)
                                      TrackedDevicePose_t* pRenderPoseArray,
                                  uint32_t unRenderPoseArrayCount,
                                  VR_ARRAY_COUNT(unGamePoseArrayCount)
                                      TrackedDevicePose_t* pGamePoseArray,
                                  uint32_t unGamePoseArrayCount) override;
  EVRCompositorError GetLastPoses(VR_ARRAY_COUNT(unRenderPoseArrayCount)
                                      TrackedDevicePose_t* pRenderPoseArray,
                                  uint32_t unRenderPoseArrayCount,
                                  VR_ARRAY_COUNT(unGamePoseArrayCount)
                                      TrackedDevicePose_t* pGamePoseArray,
                                  uint32_t unGamePoseArrayCount) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  EVRCompositorError GetLastPoseForTrackedDeviceIndex(
      TrackedDeviceIndex_t unDeviceIndex,
      TrackedDevicePose_t* pOutputPose,
      TrackedDevicePose_t* pOutputGamePose) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  EVRCompositorError Submit(
      EVREye eEye,
      const Texture_t* pTexture,
      const VRTextureBounds_t* pBounds = 0,
      EVRSubmitFlags nSubmitFlags = Submit_Default) override;
  void ClearLastSubmittedFrame() override { NOTIMPLEMENTED(); }
  void PostPresentHandoff() override;
  bool GetFrameTiming(Compositor_FrameTiming* pTiming,
                      uint32_t unFramesAgo = 0) override;
  uint32_t GetFrameTimings(Compositor_FrameTiming* pTiming,
                           uint32_t nFrames) override {
    NOTIMPLEMENTED();
    return 0;
  }
  float GetFrameTimeRemaining() override {
    NOTIMPLEMENTED();
    return 0;
  }
  void GetCumulativeStats(Compositor_CumulativeStats* pStats,
                          uint32_t nStatsSizeInBytes) override {
    NOTIMPLEMENTED();
  }
  void FadeToColor(float fSeconds,
                   float fRed,
                   float fGreen,
                   float fBlue,
                   float fAlpha,
                   bool bBackground = false) override {
    NOTIMPLEMENTED();
  }
  HmdColor_t GetCurrentFadeColor(bool bBackground = false) override {
    NOTIMPLEMENTED();
    return {};
  }
  void FadeGrid(float fSeconds, bool bFadeIn) override { NOTIMPLEMENTED(); }
  float GetCurrentGridAlpha() override {
    NOTIMPLEMENTED();
    return 0;
  }
  EVRCompositorError SetSkyboxOverride(VR_ARRAY_COUNT(unTextureCount)
                                           const Texture_t* pTextures,
                                       uint32_t unTextureCount) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  void ClearSkyboxOverride() override { NOTIMPLEMENTED(); }
  void CompositorBringToFront() override { NOTIMPLEMENTED(); }
  void CompositorGoToBack() override { NOTIMPLEMENTED(); }
  void CompositorQuit() override { NOTIMPLEMENTED(); }
  bool IsFullscreen() override {
    NOTIMPLEMENTED();
    return false;
  }
  uint32_t GetCurrentSceneFocusProcess() override {
    NOTIMPLEMENTED();
    return 0;
  }
  uint32_t GetLastFrameRenderer() override {
    NOTIMPLEMENTED();
    return 0;
  }
  bool CanRenderScene() override {
    NOTIMPLEMENTED();
    return false;
  }
  void ShowMirrorWindow() override { NOTIMPLEMENTED(); }
  void HideMirrorWindow() override { NOTIMPLEMENTED(); }
  bool IsMirrorWindowVisible() override {
    NOTIMPLEMENTED();
    return false;
  }
  void CompositorDumpImages() override { NOTIMPLEMENTED(); }
  bool ShouldAppRenderWithLowResources() override {
    NOTIMPLEMENTED();
    return false;
  }
  void ForceInterleavedReprojectionOn(bool bOverride) override {
    NOTIMPLEMENTED();
  }
  void ForceReconnectProcess() override { NOTIMPLEMENTED(); }
  void SuspendRendering(bool bSuspend) override;
  EVRCompositorError GetMirrorTextureD3D11(
      EVREye eEye,
      void* pD3D11DeviceOrResource,
      void** ppD3D11ShaderResourceView) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  void ReleaseMirrorTextureD3D11(void* pD3D11ShaderResourceView) override {
    NOTIMPLEMENTED();
  }
  EVRCompositorError GetMirrorTextureGL(
      EVREye eEye,
      glUInt_t* pglTextureId,
      glSharedTextureHandle_t* pglSharedTextureHandle) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  bool ReleaseSharedGLTexture(
      glUInt_t glTextureId,
      glSharedTextureHandle_t glSharedTextureHandle) override {
    NOTIMPLEMENTED();
    return false;
  }
  void LockGLSharedTextureForAccess(
      glSharedTextureHandle_t glSharedTextureHandle) override {
    NOTIMPLEMENTED();
  }
  void UnlockGLSharedTextureForAccess(
      glSharedTextureHandle_t glSharedTextureHandle) override {
    NOTIMPLEMENTED();
  }
  uint32_t GetVulkanInstanceExtensionsRequired(VR_OUT_STRING() char* pchValue,
                                               uint32_t unBufferSize) override {
    NOTIMPLEMENTED();
    return 0;
  }
  uint32_t GetVulkanDeviceExtensionsRequired(
      VkPhysicalDevice_T* pPhysicalDevice,
      VR_OUT_STRING() char* pchValue,
      uint32_t unBufferSize) override {
    NOTIMPLEMENTED();
    return 0;
  }
};

class TestVRClientCore : public IVRClientCore {
 public:
  EVRInitError Init(EVRApplicationType eApplicationType) override;
  void Cleanup() override;
  EVRInitError IsInterfaceVersionValid(
      const char* pchInterfaceVersion) override;
  void* GetGenericInterface(const char* pchNameAndVersion,
                            EVRInitError* peError) override;
  bool BIsHmdPresent() override;
  const char* GetEnglishStringForHmdError(EVRInitError eError) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  const char* GetIDForVRInitError(EVRInitError eError) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
};

TestVRLogger g_logger;
TestVRSystem g_system;
TestVRCompositor g_compositor;
TestVRClientCore g_loader;

EVRInitError TestVRClientCore::Init(EVRApplicationType eApplicationType) {
  g_logger.Start();
  return VRInitError_None;
}

void TestVRClientCore::Cleanup() {
  g_logger.Stop();
}

EVRInitError TestVRClientCore::IsInterfaceVersionValid(
    const char* pchInterfaceVersion) {
  return VRInitError_None;
}

void* TestVRClientCore::GetGenericInterface(const char* pchNameAndVersion,
                                            EVRInitError* peError) {
  *peError = VRInitError_None;
  if (strcmp(pchNameAndVersion, IVRSystem_Version) == 0)
    return (IVRSystem*)(&g_system);
  if (strcmp(pchNameAndVersion, IVRCompositor_Version) == 0)
    return (IVRCompositor*)(&g_compositor);

  *peError = VRInitError_Init_InvalidInterface;
  return nullptr;
}

bool TestVRClientCore::BIsHmdPresent() {
  return true;
}

void TestVRSystem::GetRecommendedRenderTargetSize(uint32_t* pnWidth,
                                                  uint32_t* pnHeight) {
  *pnWidth = 1024;
  *pnHeight = 768;
}

void TestVRSystem::GetDXGIOutputInfo(int32_t* pnAdapterIndex) {
  // Enumerate devices until we find one that supports 11.1.
  *pnAdapterIndex = -1;
  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  DCHECK(
      SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(dxgi_factory.GetAddressOf()))));
  for (int i = 0; SUCCEEDED(
           dxgi_factory->EnumAdapters(i, adapter.ReleaseAndGetAddressOf()));
       ++i) {
    D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1};
    UINT flags = 0;
    D3D_FEATURE_LEVEL feature_level_out = D3D_FEATURE_LEVEL_11_1;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
    if (SUCCEEDED(D3D11CreateDevice(
            adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, feature_levels,
            arraysize(feature_levels), D3D11_SDK_VERSION,
            d3d11_device.GetAddressOf(), &feature_level_out,
            d3d11_device_context.GetAddressOf()))) {
      *pnAdapterIndex = i;
      return;
    }
  }
}

void TestVRSystem::GetProjectionRaw(EVREye eEye,
                                    float* pfLeft,
                                    float* pfRight,
                                    float* pfTop,
                                    float* pfBottom) {
  *pfLeft = 1;
  *pfRight = 1;
  *pfTop = 1;
  *pfBottom = 1;
}

HmdMatrix34_t TestVRSystem::GetEyeToHeadTransform(EVREye eEye) {
  HmdMatrix34_t ret = {};
  ret.m[0][0] = 1;
  ret.m[1][1] = 1;
  ret.m[2][2] = 1;
  ret.m[0][3] = (eEye == Eye_Left) ? 0.1f : -0.1f;
  return ret;
}

void TestVRSystem::GetDeviceToAbsoluteTrackingPose(
    ETrackingUniverseOrigin eOrigin,
    float fPredictedSecondsToPhotonsFromNow,
    VR_ARRAY_COUNT(unTrackedDevicePoseArrayCount)
        TrackedDevicePose_t* pTrackedDevicePoseArray,
    uint32_t unTrackedDevicePoseArrayCount) {
  TrackedDevicePose_t pose = {};
  pose.mDeviceToAbsoluteTracking.m[0][0] = 1;
  pose.mDeviceToAbsoluteTracking.m[1][1] = 1;
  pose.mDeviceToAbsoluteTracking.m[2][2] = 1;
  pose.mDeviceToAbsoluteTracking.m[0][2] = 5;

  pose.vVelocity = {0, 0, 0};
  pose.vAngularVelocity = {0, 0, 0};
  pose.eTrackingResult = TrackingResult_Running_OK;
  pose.bPoseIsValid = true;
  pose.bDeviceIsConnected = true;
  pTrackedDevicePoseArray[0] = pose;

  for (unsigned int i = 1; i < unTrackedDevicePoseArrayCount; ++i) {
    TrackedDevicePose_t pose = {};
    pTrackedDevicePoseArray[i] = pose;
  }
}

bool TestVRSystem::PollNextEvent(VREvent_t*, unsigned int) {
  return false;
}

uint32_t TestVRSystem::GetStringTrackedDeviceProperty(
    TrackedDeviceIndex_t unDeviceIndex,
    ETrackedDeviceProperty prop,
    VR_OUT_STRING() char* pchValue,
    uint32_t unBufferSize,
    ETrackedPropertyError* pError) {
  if (pError) {
    *pError = TrackedProp_Success;
  }
  sprintf_s(pchValue, unBufferSize, "test-value");
  return 11;
}

float TestVRSystem::GetFloatTrackedDeviceProperty(
    TrackedDeviceIndex_t unDeviceIndex,
    ETrackedDeviceProperty prop,
    ETrackedPropertyError* pError) {
  if (pError) {
    *pError = TrackedProp_Success;
  }
  switch (prop) {
    case Prop_UserIpdMeters_Float:
      return 0.06f;
    default:
      NOTIMPLEMENTED();
  }
  return 0;
}

HmdMatrix34_t TestVRSystem::GetSeatedZeroPoseToStandingAbsoluteTrackingPose() {
  HmdMatrix34_t ret = {};
  ret.m[0][0] = 1;
  ret.m[1][1] = 1;
  ret.m[2][2] = 1;
  ret.m[1][3] = 1.2f;
  return ret;
}

void TestVRCompositor::SuspendRendering(bool bSuspend) {}

void TestVRCompositor::SetTrackingSpace(ETrackingUniverseOrigin) {}

EVRCompositorError TestVRCompositor::WaitGetPoses(TrackedDevicePose_t* poses1,
                                                  unsigned int count1,
                                                  TrackedDevicePose_t* poses2,
                                                  unsigned int count2) {
  if (poses1)
    g_system.GetDeviceToAbsoluteTrackingPose(TrackingUniverseSeated, 0, poses1,
                                             count1);

  if (poses2)
    g_system.GetDeviceToAbsoluteTrackingPose(TrackingUniverseSeated, 0, poses2,
                                             count2);

  return VRCompositorError_None;
}

EVRCompositorError TestVRCompositor::Submit(EVREye,
                                            Texture_t const* texture,
                                            VRTextureBounds_t const*,
                                            EVRSubmitFlags) {
  g_logger.OnPresentedFrame(
      reinterpret_cast<ID3D11Texture2D*>(texture->handle));
  return VRCompositorError_None;
}

void TestVRCompositor::PostPresentHandoff() {}

bool TestVRCompositor::GetFrameTiming(Compositor_FrameTiming*, unsigned int) {
  return false;
}

}  // namespace vr

extern "C" {
__declspec(dllexport) void* VRClientCoreFactory(const char* pInterfaceName,
                                                int* pReturnCode) {
  return &vr::g_loader;
}
}
