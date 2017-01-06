// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_MANAGER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"

namespace content {
class WebContents;
}

namespace vr_shell {

// Note: This class is not thread safe and must only be used from main thread.
class VrInputManager {
 public:
  explicit VrInputManager(content::WebContents* web_contents);
  ~VrInputManager();

  base::WeakPtr<VrInputManager> GetWeakPtr();
  void ProcessUpdatedGesture(std::unique_ptr<blink::WebInputEvent> event);

 private:
  void SendGesture(const blink::WebGestureEvent& gesture);
  void ForwardGestureEvent(const blink::WebGestureEvent& gesture);
  void ForwardMouseEvent(const blink::WebMouseEvent& mouse_event);

  content::WebContents* web_contents_;

  base::WeakPtrFactory<VrInputManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrInputManager);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_INPUT_MANAGER_H_
