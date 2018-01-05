// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_KEYBOARD_H_
#define CHROME_BROWSER_VR_ELEMENTS_KEYBOARD_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/keyboard_delegate.h"

namespace vr {

// Represents the virtual keyboard. This element is a proxy to the
// platform-specific keyboard implementation.
class Keyboard : public UiElement {
 public:
  Keyboard();
  ~Keyboard() override;

  void SetKeyboardDelegate(KeyboardDelegate* keyboard_delegate);
  void HitTest(const HitTestRequest& request,
               HitTestResult* result) const final;
  void NotifyClientFloatAnimated(float value,
                                 int target_property_id,
                                 cc::Animation* animation) override;

  void OnHoverEnter(const gfx::PointF& position) override;
  void OnHoverLeave() override;
  void OnMove(const gfx::PointF& position) override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;

 private:
  bool OnBeginFrame(const base::TimeTicks& time,
                    const gfx::Transform& head_pose) override;
  void OnUpdatedWorldSpaceTransform() override;
  void Render(UiElementRenderer* renderer,
              const CameraModel& camera_model) const final;
  void OnSetFocusable() override;

  void UpdateDelegateVisibility();

  KeyboardDelegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Keyboard);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_KEYBOARD_H_
