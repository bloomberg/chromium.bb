// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_command_handler_linux.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

BrowserCommandHandlerLinux::BrowserCommandHandlerLinux(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  aura::Window* window = browser_view_->frame()->GetNativeWindow();
  DCHECK(window);
  if (window)
    window->AddPreTargetHandler(this);
}

BrowserCommandHandlerLinux::~BrowserCommandHandlerLinux() {
  aura::Window* window = browser_view_->frame()->GetNativeWindow();
  if (window)
    window->RemovePreTargetHandler(this);
}

void BrowserCommandHandlerLinux::OnMouseEvent(ui::MouseEvent* event) {
  // Handle standard Linux mouse buttons for going back and forward.
  if (event->type() != ui::ET_MOUSE_PRESSED)
    return;

  // If extended mouse buttons are supported handle them in the renderer.
  if (base::FeatureList::IsEnabled(features::kExtendedMouseButtons))
    return;

  bool back_button_pressed =
      (event->changed_button_flags() == ui::EF_BACK_MOUSE_BUTTON);
  bool forward_button_pressed =
      (event->changed_button_flags() == ui::EF_FORWARD_MOUSE_BUTTON);
  if (!back_button_pressed && !forward_button_pressed)
    return;

  content::WebContents* contents =
      browser_view_->browser()->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return;
  content::NavigationController& controller = contents->GetController();
  if (back_button_pressed && controller.CanGoBack())
    controller.GoBack();
  else if (forward_button_pressed && controller.CanGoForward())
    controller.GoForward();

  // Always consume the event, whether a navigation was successful or not.
  //
  // TODO(mustaq): Perhaps we should mark "handled" only for successful
  //   navigation above but a bug in the past didn't allow it:
  //   https://codereview.chromium.org/2763313002/#msg19
  event->SetHandled();
}
