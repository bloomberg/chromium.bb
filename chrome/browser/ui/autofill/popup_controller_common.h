// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_POPUP_CONTROLLER_COMMON_H_
#define CHROME_BROWSER_UI_AUTOFILL_POPUP_CONTROLLER_COMMON_H_

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class RenderViewHost;
class WebContents;
}

namespace autofill {

// Class that controls common functionality for  Autofill style popups. Can
// determine the correct location of a popup of a desired size and can register
// a handler to key press events.
class PopupControllerCommon {
 public:
  PopupControllerCommon(const gfx::RectF& element_bounds,
                        base::i18n::TextDirection text_direction,
                        gfx::NativeView container_view,
                        content::WebContents* web_contents);
  virtual ~PopupControllerCommon();

  const gfx::RectF& element_bounds() const { return element_bounds_; }
  bool is_rtl() const { return text_direction_ == base::i18n::RIGHT_TO_LEFT; }
  gfx::NativeView container_view() { return container_view_; }
  content::WebContents* web_contents() { return web_contents_; }

  // Callback used to register with RenderViewHost. This can only be set once,
  // or else a callback may be registered that will not be removed
  // (crbug.com/338070). Call will crash if callback is already set.
  void SetKeyPressCallback(content::RenderWidgetHost::KeyPressEventCallback);

  // Register listener for key press events with the current RenderViewHost
  // associated with |web_contents_|. If callback has already been registered,
  // this has no effect.
  void RegisterKeyPressCallback();

  // Remove previously registered callback, assuming that the current
  // RenderViewHost is the same as when it was originally registered. Safe to
  // call even if the callback is not currently registered.
  void RemoveKeyPressCallback();

 private:
  // The bounds of the text element that is the focus of the popup.
  // These coordinates are in screen space.
  gfx::RectF element_bounds_;

  // The direction of the <input>.
  base::i18n::TextDirection text_direction_;

  // Weak reference
  gfx::NativeView container_view_;

  // The WebContents in which this object should listen for keyboard events
  // while showing the popup. Can be NULL, in which case this object will not
  // listen for keyboard events.
  content::WebContents* web_contents_;

  // The RenderViewHost that this object has registered its keyboard press
  // callback with.
  content::RenderViewHost* key_press_event_target_;

  content::RenderWidgetHost::KeyPressEventCallback key_press_event_callback_;

  DISALLOW_COPY_AND_ASSIGN(PopupControllerCommon);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_POPUP_CONTROLLER_COMMON_H_
