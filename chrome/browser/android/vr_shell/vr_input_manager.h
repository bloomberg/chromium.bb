// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_MANAGER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_MANAGER_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {
class WebContents;
}

namespace vr_shell {

class VrInputManager : public base::RefCountedThreadSafe<VrInputManager> {
 public:
  explicit VrInputManager(content::WebContents* web_contents);

  void ProcessUpdatedGesture(const blink::WebInputEvent& event);

 protected:
  friend class base::RefCountedThreadSafe<VrInputManager>;
  virtual ~VrInputManager();

 private:
  void SendMouseEvent(const blink::WebMouseEvent& mouse_event);
  void SendGesture(const blink::WebGestureEvent& gesture);
  void ForwardGestureEvent(const blink::WebGestureEvent& gesture);
  void ForwardMouseEvent(const blink::WebMouseEvent& mouse_event);
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
