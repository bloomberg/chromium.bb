// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/frame/touch_browser_frame_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/touch/frame/keyboard_container_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "ui/gfx/rect.h"

namespace {

const int kKeyboardHeight = 300;

PropertyAccessor<bool>* GetFocusedStateAccessor() {
  static PropertyAccessor<bool> state;
  return &state;
}

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
  registrar_.Add(this,
                 NotificationType::TAB_CONTENTS_DESTROYED,
                 NotificationService::AllSources());

  browser_view->browser()->tabstrip_model()->AddObserver(this);
}

TouchBrowserFrameView::~TouchBrowserFrameView() {
  browser_view()->browser()->tabstrip_model()->RemoveObserver(this);
}

void TouchBrowserFrameView::Layout() {
  OpaqueBrowserFrameView::Layout();

  if (!keyboard_)
    return;

  keyboard_->SetVisible(keyboard_showing_);
  keyboard_->SetBounds(GetBoundsForReservedArea());
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

  Profile* keyboard_profile = browser_view()->browser()->profile();
  DCHECK(keyboard_profile) << "Profile required for virtual keyboard.";

  keyboard_ = new KeyboardContainerView(keyboard_profile);
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

void TouchBrowserFrameView::TabSelectedAt(TabContentsWrapper* old_contents,
                                          TabContentsWrapper* new_contents,
                                          int index,
                                          bool user_gesture) {
  TabContents* contents = new_contents->tab_contents();
  bool* editable = GetFocusedStateAccessor()->GetProperty(
      contents->property_bag());
  UpdateKeyboardAndLayout(editable ? *editable : false);
}


void TouchBrowserFrameView::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  Browser* browser = browser_view()->browser();
  if (type == NotificationType::FOCUS_CHANGED_IN_PAGE) {
    // Only modify the keyboard state if the currently active tab sent the
    // notification.
    const TabContents* current_tab = browser->GetSelectedTabContents();
    TabContents* source_tab = Source<TabContents>(source).ptr();
    const bool editable = *Details<const bool>(details).ptr();

    if (current_tab == source_tab) {
      UpdateKeyboardAndLayout(editable);
    }

    // Save the state of the focused field so that the keyboard visibility
    // can be determined after tab switching.
    GetFocusedStateAccessor()->SetProperty(
        source_tab->property_bag(), editable);
  } else if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    Browser* source_browser = Browser::GetBrowserForController(
        Source<NavigationController>(source).ptr(), NULL);
    // If the Browser for the keyboard has navigated, hide the keyboard.
    if (source_browser == browser)
      UpdateKeyboardAndLayout(false);
  } else if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    GetFocusedStateAccessor()->DeleteProperty(
        Source<TabContents>(source).ptr()->property_bag());
  }
}
