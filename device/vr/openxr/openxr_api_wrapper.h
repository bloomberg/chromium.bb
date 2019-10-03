// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
#define DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_

#include <d3d11.h>
#include <stdint.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"

namespace gfx {
class Quaternion;
class Point3F;
class Size;
class Transform;
}  // namespace gfx

namespace device {

class OpenXRInputHelper;
class VRTestHook;
class ServiceTestHook;

class OpenXrApiWrapper {
 public:
  OpenXrApiWrapper();
  ~OpenXrApiWrapper();
  bool IsInitialized() const;

  static std::unique_ptr<OpenXrApiWrapper> Create();

  static bool IsHardwareAvailable();
  static bool IsApiAvailable();

  static VRTestHook* GetTestHook();

  bool session_ended() const { return session_ended_; }

  XrResult InitSession(const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device,
                       std::unique_ptr<OpenXRInputHelper>* input_helper);

  XrResult BeginFrame(Microsoft::WRL::ComPtr<ID3D11Texture2D>* texture);
  XrResult EndFrame();

  XrResult GetHeadPose(base::Optional<gfx::Quaternion>* orientation,
                       base::Optional<gfx::Point3F>* position) const;
  void GetHeadFromEyes(XrView* left, XrView* right) const;

  bool HasPosition() const;
  gfx::Size GetViewSize() const;
  XrTime GetPredictedDisplayTime() const;
  XrResult GetLuid(LUID* luid) const;
  std::string GetRuntimeName() const;
  bool GetStageParameters(XrExtent2Df* stage_bounds,
                          gfx::Transform* local_from_stage) const;

  static void DEVICE_VR_EXPORT SetTestHook(VRTestHook* hook);

 private:
  void Reset();
  bool Initialize();
  void Uninitialize();

  XrResult InitializeSystem();
  XrResult PickEnvironmentBlendMode(XrSystemId system);
  XrResult ProcessEvents();

  XrResult CreateSession(
      const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device);
  XrResult CreateSwapchain();
  XrResult CreateSpace(XrReferenceSpaceType type, XrSpace* space);
  XrResult CreateGamepadHelper(
      std::unique_ptr<OpenXRInputHelper>* input_helper);

  XrResult BeginSession();
  XrResult UpdateProjectionLayers();
  XrResult LocateViews(XrReferenceSpaceType type,
                       std::vector<XrView>* views) const;

  bool HasInstance() const;
  bool HasSystem() const;
  bool HasBlendMode() const;
  bool HasSession() const;
  bool HasColorSwapChain() const;
  bool HasSpace(XrReferenceSpaceType type) const;
  bool HasFrameState() const;

  uint32_t GetRecommendedSwapchainSampleCount() const;
  XrResult GetStageBounds(XrExtent2Df* stage_bounds) const;

  bool session_ended_;

  // Testing objects
  static VRTestHook* test_hook_;
  static ServiceTestHook* service_test_hook_;

  // OpenXR objects

  // These objects are valid on successful initialization.
  XrInstance instance_;
  XrSystemId system_;
  std::vector<XrViewConfigurationView> view_configs_;
  XrEnvironmentBlendMode blend_mode_;

  // These objects are valid only while a session is active,
  // and stay constant throughout the lifetime of a session.
  XrSession session_;
  XrSwapchain color_swapchain_;
  std::vector<XrSwapchainImageD3D11KHR> color_swapchain_images_;
  XrSpace local_space_;
  XrSpace stage_space_;
  XrSpace view_space_;

  // These objects store information about the current frame. They're
  // valid only while a session is active, and they are updated each frame.
  XrFrameState frame_state_;
  std::vector<XrView> origin_from_eye_views_;
  std::vector<XrView> head_from_eye_views_;
  std::vector<XrCompositionLayerProjectionView> layer_projection_views_;

  DISALLOW_COPY_AND_ASSIGN(OpenXrApiWrapper);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
