// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_IME_CONTROLLER_H_
#define COMPONENTS_HTML_VIEWER_IME_CONTROLLER_H_

#include "third_party/WebKit/public/web/WebTextInputInfo.h"

namespace blink {
class WebGestureEvent;
class WebWidget;
}

namespace mus {
class Window;
}

namespace html_viewer {

// This class is used by HTMLWidgetRootLocal and HTMLWidgetLocalRoot for
// handling IME related stuff.
class ImeController {
 public:
  ImeController(mus::Window* window, blink::WebWidget* widget);
  ~ImeController();

  // Methods called by WebWidget overrides.
  void ResetInputMethod();
  void DidHandleGestureEvent(const blink::WebGestureEvent& event,
                             bool event_cancelled);
  void DidUpdateTextOfFocusedElementByNonUserInput();
  void ShowImeIfNeeded();

 private:
  // Update text input state from WebWidget to mus::Window. If the focused
  // element is editable and |show_ime| is True, the software keyboard will be
  // shown.
  void UpdateTextInputState(bool show_ime);

  // Not owned objects.
  mus::Window* window_;
  blink::WebWidget* widget_;

  blink::WebTextInputInfo text_input_info_;

  DISALLOW_COPY_AND_ASSIGN(ImeController);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_IME_CONTROLLER_H_
