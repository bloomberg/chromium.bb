// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/frame/touch_browser_frame_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/touch/frame/keyboard_container_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_touch.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "views/controls/button/image_button.h"
#include "views/controls/textfield/textfield.h"
#include "views/focus/focus_manager.h"
#include "views/window/hit_test.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"
#endif

namespace {

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

// static
const char TouchBrowserFrameView::kViewClassName[] =
    "browser/ui/touch/frame/TouchBrowserFrameView";

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, public:

TouchBrowserFrameView::TouchBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : OpaqueBrowserFrameView(frame, browser_view),
      keyboard_showing_(false),
      keyboard_height_(kDefaultKeyboardHeight),
      focus_listener_added_(false),
      keyboard_(NULL) {
  registrar_.Add(this,
                 content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 NotificationService::AllSources());
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
  registrar_.Add(this,
                 chrome::NOTIFICATION_EDITABLE_ELEMENT_TOUCHED,
                 NotificationService::AllSources());

  browser_view->browser()->tabstrip_model()->AddObserver(this);

  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::LINEAR);
  animation_->SetSlideDuration(kKeyboardSlideDuration);

#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  manager->AddVirtualKeyboardObserver(this);
#endif
}

TouchBrowserFrameView::~TouchBrowserFrameView() {
  browser_view()->browser()->tabstrip_model()->RemoveObserver(this);
}

int TouchBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  if (keyboard_ && keyboard_->GetMirroredBounds().Contains(point))
    return HTCLIENT;
  return OpaqueBrowserFrameView::NonClientHitTest(point);
}

std::string TouchBrowserFrameView::GetClassName() const {
  return kViewClassName;
}

void TouchBrowserFrameView::Layout() {
  OpaqueBrowserFrameView::Layout();

  if (!keyboard_)
    return;

  keyboard_->SetVisible(keyboard_showing_ || animation_->is_animating());
  gfx::Rect bounds = GetBoundsForReservedArea();
  if (animation_->is_animating() && !keyboard_showing_) {
    // The keyboard is in the process of hiding. So pretend it still has the
    // same bounds as when the keyboard is visible. But
    // |GetBoundsForReservedArea| should not take this into account so that the
    // render view gets the entire area to relayout itself.
    bounds.set_y(bounds.y() - keyboard_height_);
    bounds.set_height(keyboard_height_);
  }
  keyboard_->SetBoundsRect(bounds);
}

void TouchBrowserFrameView::FocusWillChange(views::View* focused_before,
                                            views::View* focused_now) {
  VirtualKeyboardType before = DecideKeyboardStateForView(focused_before);
  VirtualKeyboardType now = DecideKeyboardStateForView(focused_now);
  if (before != now) {
    // TODO(varunjain): support other types of keyboard.
    UpdateKeyboardAndLayout(now == GENERIC);
  }
}

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, protected:

int TouchBrowserFrameView::GetReservedHeight() const {
  return keyboard_showing_ ? keyboard_height_ : 0;
}

void TouchBrowserFrameView::ViewHierarchyChanged(bool is_add,
                                                 View* parent,
                                                 View* child) {
  OpaqueBrowserFrameView::ViewHierarchyChanged(is_add, parent, child);
  if (!GetFocusManager())
    return;

  if (is_add && !focus_listener_added_) {
    // Add focus listener when this view is added to the hierarchy.
    GetFocusManager()->AddFocusChangeListener(this);
    focus_listener_added_ = true;
  } else if (!is_add && focus_listener_added_) {
    // Remove focus listener when this view is removed from the hierarchy.
    GetFocusManager()->RemoveFocusChangeListener(this);
    focus_listener_added_ = false;
  }
}

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, private:

void TouchBrowserFrameView::InitVirtualKeyboard() {
  if (keyboard_)
    return;

  Profile* keyboard_profile = browser_view()->browser()->profile();
  DCHECK(keyboard_profile) << "Profile required for virtual keyboard.";

  keyboard_ = new KeyboardContainerView(keyboard_profile,
                                        browser_view()->browser(),
                                        url_);
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
  if (keyboard_showing_) {
    // We don't re-layout the client view until the animation ends (see
    // AnimationEnded below) because we want the client view to occupy the
    // entire height during the animation. It is necessary to reset the
    // transform for the keyboard first so that the contents are sized properly
    // when layout, and then start the animation.
    ui::Transform reset;
    keyboard_->SetTransform(reset);
    Layout();

    animation_->Show();
  } else {
    animation_->Hide();

    browser_view()->set_clip_y(ui::Tween::ValueBetween(
          animation_->GetCurrentValue(), 0, keyboard_height_));
    parent()->Layout();
  }
}

TouchBrowserFrameView::VirtualKeyboardType
    TouchBrowserFrameView::DecideKeyboardStateForView(views::View* view) {
  if (!view)
    return NONE;

  std::string cname = view->GetClassName();
  if (cname == views::Textfield::kViewClassName) {
    return GENERIC;
  } else if (cname == RenderWidgetHostViewViews::kViewClassName) {
    TabContents* contents = browser_view()->browser()->GetSelectedTabContents();
    bool* editable = contents ? GetFocusedStateAccessor()->GetProperty(
        contents->property_bag()) : NULL;
    if (editable && *editable)
      return GENERIC;
  }
  return NONE;
}

bool TouchBrowserFrameView::HitTest(const gfx::Point& point) const {
  if (OpaqueBrowserFrameView::HitTest(point))
    return true;

  if (close_button()->IsVisible() &&
      close_button()->GetMirroredBounds().Contains(point))
    return true;
  if (restore_button()->IsVisible() &&
      restore_button()->GetMirroredBounds().Contains(point))
    return true;
  if (maximize_button()->IsVisible() &&
      maximize_button()->GetMirroredBounds().Contains(point))
    return true;
  if (minimize_button()->IsVisible() &&
      minimize_button()->GetMirroredBounds().Contains(point))
    return true;

  return false;
}

void TouchBrowserFrameView::ActiveTabChanged(TabContentsWrapper* old_contents,
                                             TabContentsWrapper* new_contents,
                                             int index,
                                             bool user_gesture) {
  TabContents* contents = new_contents->tab_contents();
  if (!TabContentsHasFocus(contents))
    return;

  bool* editable = GetFocusedStateAccessor()->GetProperty(
      contents->property_bag());
  UpdateKeyboardAndLayout(editable ? *editable : false);
}

void TouchBrowserFrameView::TabStripEmpty() {
  if (animation_->is_animating()) {
    // Reset the delegate so the AnimationEnded callback doesn't trigger.
    animation_->set_delegate(NULL);
    animation_->Stop();
  }
}

void TouchBrowserFrameView::Observe(int type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  Browser* browser = browser_view()->browser();
  if (type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE) {
    // Only modify the keyboard state if the currently active tab sent the
    // notification.
    const TabContents* current_tab = browser->GetSelectedTabContents();
    TabContents* source_tab = Source<TabContents>(source).ptr();
    const bool editable = *Details<const bool>(details).ptr();

    if (current_tab == source_tab && TabContentsHasFocus(source_tab))
      UpdateKeyboardAndLayout(editable);

    // Save the state of the focused field so that the keyboard visibility
    // can be determined after tab switching.
    GetFocusedStateAccessor()->SetProperty(
        source_tab->property_bag(), editable);
  } else if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    NavigationController* controller =
        Source<NavigationController>(source).ptr();
    Browser* source_browser = Browser::GetBrowserForController(
        controller, NULL);

    // If the Browser for the keyboard has navigated, re-evaluate the visibility
    // of the keyboard.
    TouchBrowserFrameView::VirtualKeyboardType keyboard_type = NONE;
    views::View* view = GetFocusManager()->GetFocusedView();
    if (view) {
      if (view->GetClassName() == views::Textfield::kViewClassName)
        keyboard_type = GENERIC;
      if (view->GetClassName() == RenderWidgetHostViewViews::kViewClassName) {
        // Reset the state of the focused field in the current tab.
        GetFocusedStateAccessor()->SetProperty(
            controller->tab_contents()->property_bag(), false);
      }
    }
    if (source_browser == browser)
      UpdateKeyboardAndLayout(keyboard_type == GENERIC);
  } else if (type == content::NOTIFICATION_TAB_CONTENTS_DESTROYED) {
    GetFocusedStateAccessor()->DeleteProperty(
        Source<TabContents>(source).ptr()->property_bag());
  } else if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    OpaqueBrowserFrameView::Observe(type, source, details);
  } else if (type == chrome::NOTIFICATION_HIDE_KEYBOARD_INVOKED) {
    TabContents* tab_contents =
        browser_view()->browser()->GetSelectedTabContents();
    if (tab_contents) {
      GetFocusedStateAccessor()->SetProperty(tab_contents->property_bag(),
                                             false);
    }
    UpdateKeyboardAndLayout(false);
  } else if (type == chrome::NOTIFICATION_SET_KEYBOARD_HEIGHT_INVOKED) {
    // TODO(penghuang) Allow extension conrtol the virtual keyboard directly
    // instead of using Notification.
    int height = *reinterpret_cast<int*>(details.map_key());
    if (height != keyboard_height_) {
      DCHECK_GE(height, 0) << "Height of the keyboard is less than 0.";
      DCHECK_LE(height, View::height()) << "Height of the keyboard is greater "
        "than the height of frame view.";
      keyboard_height_ = height;
      parent()->Layout();
    }
  } else if (type == chrome::NOTIFICATION_EDITABLE_ELEMENT_TOUCHED) {
    UpdateKeyboardAndLayout(true);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ui::AnimationDelegate implementation
void TouchBrowserFrameView::AnimationProgressed(const ui::Animation* anim) {
  ui::Transform transform;
  transform.SetTranslateY(
      ui::Tween::ValueBetween(anim->GetCurrentValue(), keyboard_height_, 0));
  keyboard_->SetTransform(transform);
  browser_view()->set_clip_y(
      ui::Tween::ValueBetween(anim->GetCurrentValue(), 0, keyboard_height_));
  SchedulePaint();
}

void TouchBrowserFrameView::AnimationEnded(const ui::Animation* animation) {
  browser_view()->set_clip_y(0);
  if (keyboard_showing_) {
    // Because the NonClientFrameView is a sibling of the ClientView, we rely on
    // the parent to resize the ClientView instead of resizing it directly.
    parent()->Layout();

    // The keyboard that pops up may end up hiding the text entry. So make sure
    // the renderer scrolls when necessary to keep the textfield visible.
    RenderViewHost* host =
        browser_view()->browser()->GetSelectedTabContents()->render_view_host();
    host->Send(new ViewMsg_ScrollFocusedEditableNodeIntoView(
        host->routing_id()));
  } else {
    // Notify the keyboard that it is hidden now.
    keyboard_->SetVisible(false);
  }
  SchedulePaint();
}

#if defined(OS_CHROMEOS)
void TouchBrowserFrameView::VirtualKeyboardChanged(
    chromeos::input_method::InputMethodManager* manager,
    const chromeos::input_method::VirtualKeyboard& virtual_keyboard,
    const std::string& virtual_keyboard_layout) {
  const GURL& new_url =
      virtual_keyboard.GetURLForLayout(virtual_keyboard_layout);
  if (new_url == url_)
    return;

  url_ = new_url;
  if (keyboard_)
    keyboard_->LoadURL(url_);
  // |keyboard_| can be NULL here. In that case, the |url_| will be used in
  // InitVirtualKeyboard() later.
}
#endif
