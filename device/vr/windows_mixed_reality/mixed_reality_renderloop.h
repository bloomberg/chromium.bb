// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_

#include <windows.graphics.holographic.h>
#include <windows.perception.spatial.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/win/scoped_winrt_initializer.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/windows/compositor_base.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/win/window_impl.h"

namespace device {

class MixedRealityWindow;

class MixedRealityRenderLoop : public XRCompositorCommon {
 public:
  explicit MixedRealityRenderLoop(
      base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
          on_display_info_changed);
  ~MixedRealityRenderLoop() override;

 private:
  // XRCompositorCommon:
  bool StartRuntime() override;
  void StopRuntime() override;
  void OnSessionStart() override;

  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  mojom::XRGamepadDataPtr GetNextGamepadData() override;
  void GetEnvironmentIntegrationProvider(
      mojom::XREnvironmentIntegrationProviderAssociatedRequest
          environment_provider) override;
  bool PreComposite() override;
  bool SubmitCompositedFrame() override;

  // Helpers to implement XRDeviceAbstraction.
  std::vector<mojom::XRInputSourceStatePtr> GetInputState();
  void InitializeOrigin();
  void InitializeSpace();
  void StartPresenting();
  void UpdateWMRDataForNextFrame();
  bool UpdateDisplayInfo();  // returns true if display info has changed.

  std::unique_ptr<base::win::ScopedWinrtInitializer> initializer_;

  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::IHolographicSpace>
      holographic_space_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem>
      origin_;
  std::unique_ptr<MixedRealityWindow> window_;
  mojom::VRDisplayInfoPtr current_display_info_;
  base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
      on_display_info_changed_;

  // Per frame data:
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::IHolographicFrame>
      holographic_frame_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicFramePrediction>
      prediction_;
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVectorView<
      ABI::Windows::Graphics::Holographic::HolographicCameraPose*>>
      poses_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicCameraPose>
      pose_;
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::
                             IHolographicCameraRenderingParameters>
      rendering_params_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicCamera>
      camera_;

  DISALLOW_COPY_AND_ASSIGN(MixedRealityRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_
