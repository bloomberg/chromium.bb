// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/frame/touch_browser_frame_view.h"

#include <algorithm>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "gfx/rect.h"

namespace {

const int kKeyboardHeight = 300;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, public:

TouchBrowserFrameView::TouchBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : OpaqueBrowserFrameView(frame, browser_view),
      keyboard_showing_(false),
      keyboard_(NULL) {
  registrar_.Add(this,
                 NotificationType::NAV_ENTRY_COMMITTED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::FOCUS_CHANGED_IN_PAGE,
                 NotificationService::AllSources());
}

TouchBrowserFrameView::~TouchBrowserFrameView() {
}

void TouchBrowserFrameView::Layout() {
  OpaqueBrowserFrameView::Layout();

  if (!keyboard_)
    return;

  keyboard_->SetBounds(GetBoundsForReservedArea());
  keyboard_->SetVisible(keyboard_showing_);
  keyboard_->Layout();
}

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, protected:
int TouchBrowserFrameView::GetReservedHeight() const {
  if (keyboard_showing_)
    return kKeyboardHeight;

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, private:

void TouchBrowserFrameView::InitVirtualKeyboard() {
  if (keyboard_)
    return;

  keyboard_ = new DOMView;

  Profile* keyboard_profile = browser_view()->browser()->profile();
  DCHECK(keyboard_profile) << "Profile required for virtual keyboard.";

  GURL keyboard_url(chrome::kChromeUIKeyboardURL);
  keyboard_->Init(keyboard_profile,
      SiteInstance::CreateSiteInstanceForURL(keyboard_profile, keyboard_url));
  keyboard_->LoadURL(keyboard_url);

  keyboard_->SetVisible(false);
  AddChildView(keyboard_);
}

void TouchBrowserFrameView::UpdateKeyboardAndLayout(bool should_show_keyboard) {
  if (should_show_keyboard)
    InitVirtualKeyboard();

  if (should_show_keyboard == keyboard_showing_)
    return;

  DCHECK(keyboard_);

  keyboard_showing_ = should_show_keyboard;

  // Because the NonClientFrameView is a sibling of the ClientView, we rely on
  // the parent to resize the ClientView instead of resizing it directly.
  GetParent()->Layout();

  // The keyboard that pops up may end up hiding the text entry. So make sure
  // the renderer scrolls when necessary to keep the textfield visible.
  if (keyboard_showing_) {
    RenderViewHost* host =
        browser_view()->browser()->GetSelectedTabContents()->render_view_host();
    host->ScrollFocusedEditableNodeIntoView();
  }
}

void TouchBrowserFrameView::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  Browser* browser = browser_view()->browser();
  if (type == NotificationType::FOCUS_CHANGED_IN_PAGE) {
    // Only modify the keyboard state if the currently active tab sent the
    // notification.
    if (browser->GetSelectedTabContents()->render_view_host() ==
        Source<RenderViewHost>(source).ptr()) {
      UpdateKeyboardAndLayout(*Details<const bool>(details).ptr());
    }
  } else if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    Browser* source_browser = Browser::GetBrowserForController(
        Source<NavigationController>(source).ptr(), NULL);
    // If the Browser for the keyboard has navigated, hide the keyboard.
    if (source_browser == browser) {
      UpdateKeyboardAndLayout(false);
    }
  }
}
