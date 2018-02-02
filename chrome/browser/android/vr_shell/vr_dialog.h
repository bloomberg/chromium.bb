// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_DIALOG_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_DIALOG_H_

#include <jni.h>

#include <map>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

using base::android::JavaParamRef;

namespace vr_shell {
class VrDialog : public vr::ContentInputDelegate {
 public:
  VrDialog(int width, int height);
  ~VrDialog() override;

  void SetEventForwarder(vr::ContentInputForwarder* dialog);
  void SetSize(int width, int height);

 private:
  vr::ContentInputForwarder* dialog_ = nullptr;

  // vr::ContentInputDelegate.
  void OnContentEnter(const gfx::PointF& normalized_hit_point) override;
  void OnContentLeave() override;
  void OnContentMove(const gfx::PointF& normalized_hit_point) override;
  void OnContentDown(const gfx::PointF& normalized_hit_point) override;
  void OnContentUp(const gfx::PointF& normalized_hit_point) override;
  void OnContentFlingStart(std::unique_ptr<blink::WebGestureEvent> gesture,
                           const gfx::PointF& normalized_hit_point) override;
  void OnContentFlingCancel(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollUpdate(std::unique_ptr<blink::WebGestureEvent> gesture,
                             const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollEnd(std::unique_ptr<blink::WebGestureEvent> gesture,
                          const gfx::PointF& normalized_hit_point) override;
  void SendGestureToDialog(std::unique_ptr<blink::WebInputEvent> event);

  std::unique_ptr<blink::WebMouseEvent> MakeMouseEvent(
      blink::WebInputEvent::Type type,
      const gfx::PointF& normalized_web_content_location);
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(VrDialog);
};

}  //  namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_DIALOG_H_
