// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_MANAGER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_MANAGER_H_

#include "base/memory/ref_counted.h"
#include "base/task_runner_util.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/android/vr_shell/vr_gesture.h"
#include "chrome/browser/android/vr_shell/vr_input_manager.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_controller.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

class VrControllerManager
    : public base::RefCountedThreadSafe<VrControllerManager> {
 public:
  // Controller API entry point.
  VrControllerManager();

  // Must be called when the GL renderer gets onSurfaceCreated().
  std::unique_ptr<VrController> InitializeController(gvr_context_* gvr_context);

  // Must be called when the GL renderer gets onDrawFrame().
  VrGesture Update();

  // Must be called when the GL renderer gets onDrawFrame().
  void ProcessUpdatedContentGesture(VrGesture gesture);

  // Must be called when the GL renderer gets onDrawFrame().
  void ProcessUpdatedUIGesture(VrGesture gesture);

  void SetWebContents(content::WebContents* content_wc_ptr,
                      content::WebContents* ui_wc_ptr);

 protected:
  friend class base::RefCountedThreadSafe<VrControllerManager>;
  virtual ~VrControllerManager();

 private:
  void SendGesture(VrGesture gesture, VrInputManager* vr_input_manager);

  VrInputManager* vr_content_ptr_;
  VrInputManager* vr_ui_ptr_;
  std::unique_ptr<gvr::ControllerApi> controller_api_;

  DISALLOW_COPY_AND_ASSIGN(VrControllerManager);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_MANAGER_H_
