// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/touch_login_view.h"

#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#include "chrome/browser/ui/touch/frame/keyboard_container_view.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_touch.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/transform.h"
#include "views/controls/textfield/textfield.h"
#include "views/widget/widget.h"

namespace {

const char kViewClassName[] = "browser/chromeos/login/TouchLoginView";
const int kDefaultKeyboardHeight = 300;
const int kKeyboardSlideDuration = 300;  // In milliseconds

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
      keyboard_height_(kDefaultKeyboardHeight),
      focus_listener_added_(false),
      keyboard_(NULL) {
}

TouchLoginView::~TouchLoginView() {
}

void TouchLoginView::Init() {
  WebUILoginView::Init();
  InitStatusArea();
  InitVirtualKeyboard();

  registrar_.Add(this,
                 content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_HIDE_KEYBOARD_INVOKED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SET_KEYBOARD_HEIGHT_INVOKED,
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

void TouchLoginView::OnWindowCreated() {
}

// TouchLoginView protected: ---------------------------------------------------

void TouchLoginView::Layout() {
  WebUILoginView::Layout();
  DCHECK(status_area_);

  // Layout the Status Area up in the right corner. This should always be done.
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(
      width() - status_area_size.width() -
          WebUILoginView::kStatusAreaCornerPadding,
      WebUILoginView::kStatusAreaCornerPadding,
      status_area_size.width(),
      status_area_size.height());

  if (!keyboard_)
    return;

  // We are not resizing the DOMView here, so the keyboard is going to occlude
  // the login screen partially. It is the responsibility of the UX layer to
  // handle this.

  // Lastly layout the keyboard
  bool display_keyboard = (keyboard_showing_ || animation_->is_animating());
  keyboard_->SetVisible(display_keyboard);
  gfx::Rect keyboard_bounds = bounds();
  int keyboard_height = display_keyboard ? keyboard_height_ : 0;
  keyboard_bounds.set_y(keyboard_bounds.height() - keyboard_height);
  keyboard_bounds.set_height(keyboard_height);
  keyboard_->SetBoundsRect(keyboard_bounds);
}

void TouchLoginView::InitStatusArea() {
  if (status_area_)
    return;
  status_area_ = new StatusAreaView(this);
  status_area_->Init();
  AddChildView(status_area_);
}

// TouchLoginView private: -----------------------------------------------------

void TouchLoginView::InitVirtualKeyboard() {
  keyboard_ = new KeyboardContainerView(profile_, NULL);
  keyboard_->SetVisible(false);
  AddChildView(keyboard_);

  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::LINEAR);
  animation_->SetSlideDuration(kKeyboardSlideDuration);
}

void TouchLoginView::UpdateKeyboardAndLayout(bool should_show_keyboard) {
  DCHECK(keyboard_);
  if (should_show_keyboard == keyboard_showing_)
    return;
  keyboard_showing_ = should_show_keyboard;
  if (keyboard_showing_) {
    ui::Transform transform;
    transform.SetTranslateY(-keyboard_height_);
    keyboard_->SetTransform(transform);
    Layout();
    animation_->Show();
  } else {
    ui::Transform transform;
    keyboard_->SetTransform(transform);
    animation_->Hide();
    Layout();
  }
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

void TouchLoginView::Observe(int type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE) {
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
  } else if (type == content::NOTIFICATION_TAB_CONTENTS_DESTROYED) {
    GetFocusedStateAccessor()->DeleteProperty(
        Source<TabContents>(source).ptr()->property_bag());
  } else if (type == chrome::NOTIFICATION_HIDE_KEYBOARD_INVOKED) {
    UpdateKeyboardAndLayout(false);
  } else if (type == chrome::NOTIFICATION_SET_KEYBOARD_HEIGHT_INVOKED) {
    // TODO(penghuang) Allow extension conrtol the virtual keyboard directly
    // instead of using Notification.
    int height = *(Details<int>(details).ptr());
    if (height != keyboard_height_) {
      DCHECK_GE(height, 0) << "Height of the keyboard is less than 0.";
      DCHECK_LE(height, View::height()) << "Height of the keyboard is greater "
          "than the height of containing view.";
      keyboard_height_ = height;
      Layout();
    }
  }
}

// ui::AnimationDelegate implementation ----------------------------------------

void TouchLoginView::AnimationProgressed(const ui::Animation* anim) {
  ui::Transform transform;
  transform.SetTranslateY(
      ui::Tween::ValueBetween(anim->GetCurrentValue(), keyboard_height_, 0));
  keyboard_->SetTransform(transform);
}

void TouchLoginView::AnimationEnded(const ui::Animation* animation) {
  if (keyboard_showing_) {
    Layout();
  } else {
    // Notify the keyboard that it is hidden now.
    keyboard_->SetVisible(false);
  }
}

}  // namespace chromeos
