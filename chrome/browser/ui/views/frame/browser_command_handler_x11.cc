// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_command_handler_x11.h"

#include <X11/Xlib.h>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

BrowserCommandHandlerX11::BrowserCommandHandlerX11(Browser* browser)
    : browser_(browser) {}

BrowserCommandHandlerX11::~BrowserCommandHandlerX11() {}

void BrowserCommandHandlerX11::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_PRESSED)
    return;
  XEvent* xevent = event->native_event();
  if (!xevent)
    return;
  int button = xevent->type == GenericEvent ? ui::EventButtonFromNative(xevent)
                                            : xevent->xbutton.button;

  // Standard Linux mouse buttons for going back and forward.
  const int kBackMouseButton = 8;
  const int kForwardMouseButton = 9;
  if (button == kBackMouseButton || button == kForwardMouseButton) {
    content::WebContents* contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    if (!contents)
      return;
    content::NavigationController& controller = contents->GetController();
    if (button == kBackMouseButton && controller.CanGoBack())
      controller.GoBack();
    else if (button == kForwardMouseButton && controller.CanGoForward())
      controller.GoForward();
    // Always consume the event, whether a navigation was successful or not.
    event->SetHandled();
  }
}
