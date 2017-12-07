// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_GVR_KEYBOARD_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_GVR_KEYBOARD_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/keyboard_ui_interface.h"
#include "third_party/gvr-android-keyboard/src/libraries/headers/vr/gvr/capi/include/gvr_keyboard.h"

namespace vr {
struct TextInputInfo;
}

namespace vr_shell {

class GvrKeyboardDelegate : public vr::KeyboardDelegate {
 public:
  // Constructs a GvrKeyboardDelegate by dynamically loading the GVR keyboard
  // api. A null pointer is returned upon failure.
  static std::unique_ptr<GvrKeyboardDelegate> Create();
  ~GvrKeyboardDelegate() override;

  void SetUiInterface(vr::KeyboardUiInterface* ui);

  typedef int32_t EventType;
  typedef base::RepeatingCallback<void(EventType)> OnEventCallback;

  // vr::KeyboardDelegate implementation.
  void OnBeginFrame() override;
  void ShowKeyboard() override;
  void HideKeyboard() override;
  void SetTransform(const gfx::Transform& transform) override;
  bool HitTest(const gfx::Point3F& ray_origin,
               const gfx::Point3F& ray_target,
               gfx::Point3F* hit_position) override;
  void Draw(const vr::CameraModel& model) override;

  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;

  // Called to update GVR keyboard with the given text input info.
  void UpdateInput(const vr::TextInputInfo& info);

 private:
  GvrKeyboardDelegate();
  void Init(gvr_keyboard_context* keyboard_context);
  void OnGvrKeyboardEvent(EventType);
  vr::TextInputInfo GetTextInfo();

  vr::KeyboardUiInterface* ui_;
  gvr_keyboard_context* gvr_keyboard_ = nullptr;
  OnEventCallback keyboard_event_callback_;

  DISALLOW_COPY_AND_ASSIGN(GvrKeyboardDelegate);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_GVR_KEYBOARD_DELEGATE_H_
