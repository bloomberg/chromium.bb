// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
#define DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "device/vr/windows/compositor_base.h"

struct XrView;

namespace device {

class OpenXrApiWrapper;
class OpenXRInputHelper;

class OpenXrRenderLoop : public XRCompositorCommon {
 public:
  OpenXrRenderLoop(base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
                       on_display_info_changed);
  ~OpenXrRenderLoop() override;

  gfx::Size GetViewSize() const;

 private:
  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  mojom::XRGamepadDataPtr GetNextGamepadData() override;
  bool StartRuntime() override;
  void StopRuntime() override;
  void OnSessionStart() override;
  bool PreComposite() override;
  bool HasSessionEnded() override;
  bool SubmitCompositedFrame() override;

  bool UpdateDisplayInfo();
  bool UpdateEyeParameters();
  bool UpdateEye(const XrView& view_head,
                 const gfx::Size& view_size,
                 mojom::VREyeParametersPtr* eye) const;
  bool UpdateStageParameters();

  std::unique_ptr<OpenXrApiWrapper> openxr_;
  std::unique_ptr<OpenXRInputHelper> input_helper_;

  base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
      on_display_info_changed_;
  mojom::VRDisplayInfoPtr current_display_info_;

  DISALLOW_COPY_AND_ASSIGN(OpenXrRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
