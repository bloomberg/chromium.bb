// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_GL_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_GL_BROWSER_INTERFACE_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "device/vr/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace blink {
class WebInputEvent;
}

namespace vr_shell {

// An interface for the GL thread to communicate with VrShell. Many of the
// functions in this interface are proxies to methods on VrShell.
class GlBrowserInterface {
 public:
  virtual ~GlBrowserInterface() = default;

  virtual void ContentSurfaceChanged(jobject surface) = 0;
  virtual void GvrDelegateReady(gvr::ViewerType viewer_type) = 0;
  virtual void UpdateGamepadData(device::GvrGamepadData) = 0;
  virtual void AppButtonGesturePerformed(UiInterface::Direction direction) = 0;
  virtual void AppButtonClicked() = 0;
  virtual void ProcessContentGesture(
      std::unique_ptr<blink::WebInputEvent> event) = 0;
  virtual void ForceExitVr() = 0;
  virtual void RunVRDisplayInfoCallback(
      const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
      device::mojom::VRDisplayInfoPtr* info) = 0;
  virtual void OnContentPaused(bool enabled) = 0;
  virtual void ToggleCardboardGamepad(bool enabled) = 0;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_GL_BROWSER_INTERFACE_H_
