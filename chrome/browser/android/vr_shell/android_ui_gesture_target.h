// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_ANDROID_UI_GESTURE_TARGET_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_ANDROID_UI_GESTURE_TARGET_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"

namespace vr_shell {

class AndroidUiGestureTarget {
 public:
  AndroidUiGestureTarget(jobject event_synthesizer, float dpr_ratio);
  ~AndroidUiGestureTarget();

  void DispatchWebInputEvent(std::unique_ptr<blink::WebInputEvent> event);

 private:
  // Enum values below need to be kept in sync with MotionEventSynthesizer.java.
  enum Action {
    Invalid = -1,
    Start = 0,
    Move = 1,
    Cancel = 2,
    End = 3,
    Scroll = 4,
    HoverEnter = 5,
    HoverExit = 6,
    HoverMove = 7,
  };

  void SetPointer(JNIEnv* env, int x, int y);
  void SetScrollDeltas(JNIEnv* env, int x, int y, int dx, int dy);
  void Inject(JNIEnv* env, Action action, double time_in_seconds);

  jobject event_synthesizer_;
  int scroll_x_ = 0;
  int scroll_y_ = 0;
  float scroll_ratio_;

  DISALLOW_COPY_AND_ASSIGN(AndroidUiGestureTarget);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_ANDROID_UI_GESTURE_TARGET_H_
