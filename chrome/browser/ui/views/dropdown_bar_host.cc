// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_host.h"

#include <algorithm>

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "chrome/browser/ui/views/dropdown_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/widget.h"

// static
bool DropdownBarHost::disable_animations_during_testing_ = false;

////////////////////////////////////////////////////////////////////////////////
// DropdownBarHost, public:

DropdownBarHost::DropdownBarHost(BrowserView* browser_view)
    : browser_view_(browser_view),
      view_(NULL),
      delegate_(NULL),
      animation_offset_(0),
      focus_manager_(NULL),
      esc_accel_target_registered_(false),
      is_visible_(false) {
}

void DropdownBarHost::Init(views::View* host_view,
                           views::View* view,
                           DropdownBarHostDelegate* delegate) {
  DCHECK(view);
  DCHECK(delegate);

  view_ = view;
  delegate_ = delegate;

  // Initialize the host.
  host_.reset(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = browser_view_->GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  host_->Init(params);
  host_->SetContentsView(view_);

  SetHostViewNative(host_view);

  // Start listening to focus changes, so we can register and unregister our
  // own handler for Escape.
  focus_manager_ = host_->GetFocusManager();
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  } else {
    // In some cases (see bug http://crbug.com/17056) it seems we may not have
    // a focus manager.  Please reopen the bug if you hit this.
    NOTREACHED();
  }

  // Start the process of animating the opening of the widget.
  animation_.reset(new gfx::SlideAnimation(this));
}

DropdownBarHost::~DropdownBarHost() {
  focus_manager_->RemoveFocusChangeListener(this);
  focus_tracker_.reset(NULL);
}

void DropdownBarHost::Show(bool animate) {
  // Stores the currently focused view, and tracks focus changes so that we can
  // restore focus when the dropdown widget is closed.
  focus_tracker_.reset(new views::ExternalFocusTracker(view_, focus_manager_));

  bool was_visible = is_visible_;
  is_visible_ = true;
  if (!animate || disable_animations_during_testing_) {
    animation_->Reset(1);
    AnimationProgressed(animation_.get());
  } else if (!was_visible) {
    // Don't re-start the animation.
    animation_->Reset();
    animation_->Show();
  }

  if (!was_visible)
    OnVisibilityChanged();
}

void DropdownBarHost::SetFocusAndSelection() {
  delegate_->SetFocusAndSelection(true);
}

bool DropdownBarHost::IsAnimating() const {
  return animation_->is_animating();
}

void DropdownBarHost::Hide(bool animate) {
  if (!IsVisible())
    return;
  if (animate && !disable_animations_during_testing_ &&
      !animation_->IsClosing()) {
    animation_->Hide();
  } else {
    if (animation_->IsClosing()) {
      // If we're in the middle of a close animation, skip immediately to the
      // end of the animation.
      StopAnimation();
    } else {
      // Otherwise we need to set both the animation state to ended and the
      // DropdownBarHost state to ended/hidden, otherwise the next time we try
      // to show the bar, it might refuse to do so. Note that we call
      // AnimationEnded ourselves as Reset does not call it if we are not
      // animating here.
      animation_->Reset();
      AnimationEnded(animation_.get());
    }
  }
}

void DropdownBarHost::StopAnimation() {
  animation_->End();
}

bool DropdownBarHost::IsVisible() const {
  return is_visible_;
}

////////////////////////////////////////////////////////////////////////////////
// DropdownBarHost, views::FocusChangeListener implementation:
void DropdownBarHost::OnWillChangeFocus(views::View* focused_before,
                                        views::View* focused_now) {
  // First we need to determine if one or both of the views passed in are child
  // views of our view.
  bool our_view_before = focused_before && view_->Contains(focused_before);
  bool our_view_now = focused_now && view_->Contains(focused_now);

  // When both our_view_before and our_view_now are false, it means focus is
  // changing hands elsewhere in the application (and we shouldn't do anything).
  // Similarly, when both are true, focus is changing hands within the dropdown
  // widget (and again, we should not do anything). We therefore only need to
  // look at when we gain initial focus and when we loose it.
  if (!our_view_before && our_view_now) {
    // We are gaining focus from outside the dropdown widget so we must register
    // a handler for Escape.
    RegisterAccelerators();
  } else if (our_view_before && !our_view_now) {
    // We are losing focus to something outside our widget so we restore the
    // original handler for Escape.
    UnregisterAccelerators();
  }
}

void DropdownBarHost::OnDidChangeFocus(views::View* focused_before,
                                       views::View* focused_now) {
}

////////////////////////////////////////////////////////////////////////////////
// DropdownBarHost, gfx::AnimationDelegate implementation:

void DropdownBarHost::AnimationProgressed(const gfx::Animation* animation) {
  // First, we calculate how many pixels to slide the widget.
  gfx::Size pref_size = view_->GetPreferredSize();
  animation_offset_ = static_cast<int>((1.0 - animation_->GetCurrentValue()) *
                                       pref_size.height());

  // This call makes sure it appears in the right location, the size and shape
  // is correct and that it slides in the right direction.
  gfx::Rect dlg_rect = GetDialogPosition(gfx::Rect());
  SetDialogPosition(dlg_rect, false);

  // Let the view know if we are animating, and at which offset to draw the
  // edges.
  delegate_->SetAnimationOffset(animation_offset_);
  view_->SchedulePaint();
}

void DropdownBarHost::AnimationEnded(const gfx::Animation* animation) {
  // Place the dropdown widget in its fully opened state.
  animation_offset_ = 0;

  if (!animation_->IsShowing()) {
    // Animation has finished closing.
    host_->Hide();
    is_visible_ = false;
    OnVisibilityChanged();
  } else {
    // Animation has finished opening.
  }
}

////////////////////////////////////////////////////////////////////////////////
// DropdownBarHost protected:

void DropdownBarHost::ResetFocusTracker() {
  focus_tracker_.reset(NULL);
}

void DropdownBarHost::OnVisibilityChanged() {
}

void DropdownBarHost::GetWidgetBounds(gfx::Rect* bounds) {
  DCHECK(bounds);
  *bounds = browser_view_->bounds();
}

void DropdownBarHost::RegisterAccelerators() {
  DCHECK(!esc_accel_target_registered_);
  ui::Accelerator escape(ui::VKEY_ESCAPE, ui::EF_NONE);
  focus_manager_->RegisterAccelerator(
      escape, ui::AcceleratorManager::kNormalPriority, this);
  esc_accel_target_registered_ = true;
}

void DropdownBarHost::UnregisterAccelerators() {
  DCHECK(esc_accel_target_registered_);
  ui::Accelerator escape(ui::VKEY_ESCAPE, ui::EF_NONE);
  focus_manager_->UnregisterAccelerator(escape, this);
  esc_accel_target_registered_ = false;
}
