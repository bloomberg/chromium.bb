// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/touch_login_view.h"

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#include "chrome/browser/ui/touch/frame/keyboard_container_view.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_touch.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "views/controls/textfield/textfield.h"
#include "views/widget/widget.h"

namespace {

const char kViewClassName[] = "browser/chromeos/login/TouchLoginView";
const int kKeyboardHeight = 360;
const int kKeyboardSlideDuration = 500;  // In milliseconds

PropertyAccessor<bool>* GetFocusedStateAccessor() {
  static PropertyAccessor<bool> state;
  return &state;
}

bool TabContentsHasFocus(const TabContents* contents) {
  views::View* view = static_cast<TabContentsViewTouch*>(contents->view());
  return view->Contains(view->GetFocusManager()->GetFocusedView());
}

}  // namespace

namespace chromeos {

// TouchLoginView public: ------------------------------------------------------

TouchLoginView::TouchLoginView()
    : WebUILoginView(),
      keyboard_showing_(false),
      focus_listener_added_(false),
      keyboard_(NULL) {
}

TouchLoginView::~TouchLoginView() {
}

void TouchLoginView::Init() {
  WebUILoginView::Init();
  InitVirtualKeyboard();

  registrar_.Add(this,
                 NotificationType::FOCUS_CHANGED_IN_PAGE,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::TAB_CONTENTS_DESTROYED,
                 NotificationService::AllSources());
}

std::string TouchLoginView::GetClassName() const {
  return kViewClassName;
}

void TouchLoginView::FocusWillChange(views::View* focused_before,
                                     views::View* focused_now) {
  VirtualKeyboardType before = DecideKeyboardStateForView(focused_before);
  VirtualKeyboardType now = DecideKeyboardStateForView(focused_now);
  if (before != now) {
    // TODO(varunjain): support other types of keyboard.
    UpdateKeyboardAndLayout(now == GENERIC);
  }
}

// TouchLoginView protected: ---------------------------------------------------

void TouchLoginView::Layout() {
  WebUILoginView::Layout();
  if (!keyboard_)
    return;

  // We need to change the layout of the login screen DOMView, so that the page
  // isn't occluded by the keyboard
  if (keyboard_showing_) {
    gfx::Rect webui_login_bounds = bounds();
    webui_login_bounds.set_height(webui_login_bounds.height()
                                  - kKeyboardHeight);
    webui_login_->SetBoundsRect(webui_login_bounds);
  }

  // Lastly layout the keyboard
  keyboard_->SetVisible(keyboard_showing_);
  if (keyboard_showing_) {
    gfx::Rect keyboard_bounds = bounds();
    keyboard_bounds.set_y(keyboard_bounds.height() - kKeyboardHeight);
    keyboard_bounds.set_height(kKeyboardHeight);
    keyboard_->SetBoundsRect(keyboard_bounds);
  }
}

// TouchLoginView private: -----------------------------------------------------

void TouchLoginView::InitVirtualKeyboard() {
  keyboard_ = new KeyboardContainerView(profile_, NULL);
  keyboard_->SetVisible(false);
  AddChildView(keyboard_);
}

void TouchLoginView::UpdateKeyboardAndLayout(bool should_show_keyboard) {
  DCHECK(keyboard_);
  if (should_show_keyboard == keyboard_showing_)
    return;
  keyboard_showing_ = should_show_keyboard;
  Layout();
}

TouchLoginView::VirtualKeyboardType
    TouchLoginView::DecideKeyboardStateForView(views::View* view) {
  if (!view)
    return NONE;

  std::string cname = view->GetClassName();
  if (cname == views::Textfield::kViewClassName) {
    return GENERIC;
  } else if (cname == RenderWidgetHostViewViews::kViewClassName) {
    TabContents* contents = webui_login_->tab_contents();
    bool* editable = contents ? GetFocusedStateAccessor()->GetProperty(
        contents->property_bag()) : NULL;
    if (editable && *editable)
      return GENERIC;
  }
  return NONE;
}

void TouchLoginView::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::FOCUS_CHANGED_IN_PAGE) {
    // Only modify the keyboard state if the currently active tab sent the
    // notification.
    const TabContents* current_tab = webui_login_->tab_contents();
    TabContents* source_tab = Source<TabContents>(source).ptr();
    const bool editable = *Details<const bool>(details).ptr();

    if (current_tab == source_tab && TabContentsHasFocus(source_tab))
      UpdateKeyboardAndLayout(editable);

    // Save the state of the focused field so that the keyboard visibility
    // can be determined after tab switching.
    GetFocusedStateAccessor()->SetProperty(
        source_tab->property_bag(), editable);
  } else if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    GetFocusedStateAccessor()->DeleteProperty(
        Source<TabContents>(source).ptr()->property_bag());
  }
}

}  // namespace chromeos
