// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_MANAGER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_MANAGER_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/android/vr_shell/vr_gesture.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {
class WebContents;
}

namespace vr_shell {

class VrInputManager : public base::RefCountedThreadSafe<VrInputManager> {
 public:
  explicit VrInputManager(content::WebContents* web_contents);

  void ProcessUpdatedGesture(VrGesture gesture);

  void SendScrollEvent(int64_t time_ms,
                       float x,
                       float y,
                       float dx,
                       float dy,
                       int type);
  void SendClickEvent(int64_t time_ms, float x, float y);
  void SendMouseEvent(int64_t time_ms, float x, float y,
                      WebInputEvent::Type type);

  void ScrollBegin(int64_t time_ms,
                   float x,
                   float y,
                   float hintx,
                   float hinty,
                   bool target_viewport);
  void ScrollEnd(int64_t time_ms);
  void ScrollBy(int64_t time_ms, float x, float y, float dx, float dy);
  void PinchBegin(int64_t time_ms, float x, float y);
  void PinchEnd(int64_t time_ms);
  void PinchBy(int64_t time_ms, float x, float y, float delta);
  void SendPinchEvent(int64_t time_ms, float x, float y, float dz, int type);

 protected:
  friend class base::RefCountedThreadSafe<VrInputManager>;
  virtual ~VrInputManager();

 private:
  void SendGesture(VrGesture gesture);
  void ForwardGestureEvent(const blink::WebGestureEvent& event);
  void ForwardMouseEvent(const blink::WebMouseEvent& event);
  blink::WebGestureEvent MakeGestureEvent(blink::WebInputEvent::Type type,
                                          int64_t time_ms,
                                          float x,
                                          float y) const;

  // Device scale factor.
  float dpi_scale_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(VrInputManager);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_MANAGER_H_
