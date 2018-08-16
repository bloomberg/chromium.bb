// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_CONTROLLER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/vr/vr_controller.h"
#include "chrome/browser/vr/controller_delegate.h"

namespace gvr {
class GvrApi;
}

namespace vr {

class GestureDetector;
class GlBrowserInterface;

class GvrControllerDelegate : public ControllerDelegate {
 public:
  GvrControllerDelegate(gvr::GvrApi* gvr_api, GlBrowserInterface* browser);
  ~GvrControllerDelegate() override;

  // ControllerDelegate implementation.
  void UpdateController(const RenderInfo& render_info,
                        base::TimeTicks current_time,
                        bool is_webxr_frame) override;
  ControllerModel GetModel(const RenderInfo& render_info) override;
  InputEventList GetGestures(base::TimeTicks current_time) override;
  device::mojom::XRInputSourceStatePtr GetInputSourceState() override;
  void OnResume() override;
  void OnPause() override;

 private:
  std::unique_ptr<VrController> controller_;
  GestureDetector gesture_detector_;
  GlBrowserInterface* browser_;

  bool was_select_button_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(GvrControllerDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GVR_CONTROLLER_DELEGATE_H_
