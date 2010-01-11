// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/dropdown_bar_host.h"

#include "app/slide_animation.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/dropdown_bar_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "views/focus/external_focus_tracker.h"
#include "views/focus/view_storage.h"
#include "views/widget/widget.h"

// static
bool DropdownBarHost::disable_animations_during_testing_ = false;

////////////////////////////////////////////////////////////////////////////////
// DropdownBarHost, public:

DropdownBarHost::DropdownBarHost(BrowserView* browser_view)
    : browser_view_(browser_view),
      animation_offset_(0),
      esc_accel_target_registered_(false),
      is_visible_(false) {
}

void DropdownBarHost::Init(DropdownBarView* view) {
  view_ = view;

  // Initialize the host.
  host_.reset(CreateHost());
  host_->Init(GetNativeView(browser_view_), gfx::Rect());
  host_->SetContentsView(view_);

  // Start listening to focus changes, so we can register and unregister our
  // own handler for Escape.
  focus_manager_ =
      views::FocusManager::GetFocusManagerForNativeView(host_->GetNativeView());
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  } else {
    // In some cases (see bug http://crbug.com/17056) it seems we may not have
    // a focus manager.  Please reopen the bug if you hit this.
    NOTREACHED();
  }

  // Start the process of animating the opening of the widget.
  animation_.reset(new SlideAnimation(this));
}

DropdownBarHost::~DropdownBarHost() {
  focus_manager_->RemoveFocusChangeListener(this);
  focus_tracker_.reset(NULL);
}

void DropdownBarHost::Show(bool animate) {
  // Stores the currently focused view, and tracks focus changes so that we can
  // restore focus when the dropdown widget is closed.
  focus_tracker_.reset(new views::ExternalFocusTracker(view_, focus_manager_));

  if (!animate || disable_animations_during_testing_) {
    is_visible_ = true;
    animation_->Reset(1);
    AnimationProgressed(animation_.get());
  } else {
    if (!is_visible_) {
      // Don't re-start the animation.
      is_visible_ = true;
      animation_->Reset();
      animation_->Show();
    }
  }
}

void DropdownBarHost::SetFocusAndSelection() {
  view_->SetFocusAndSelection();
}

bool DropdownBarHost::IsAnimating() const {
  return animation_->IsAnimating();
}

void DropdownBarHost::Hide(bool animate) {
  if (!IsVisible())
    return;
  if (animate && !disable_animations_during_testing_) {
    animation_->Reset(1.0);
    animation_->Hide();
  } else {
    StopAnimation();
    is_visible_ = false;
    host_->Hide();
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
void DropdownBarHost::FocusWillChange(views::View* focused_before,
                                      views::View* focused_now) {
  // First we need to determine if one or both of the views passed in are child
  // views of our view.
  bool our_view_before = focused_before && view_->IsParentOf(focused_before);
  bool our_view_now = focused_now && view_->IsParentOf(focused_now);

  // When both our_view_before and our_view_now are false, it means focus is
  // changing hands elsewhere in the application (and we shouldn't do anything).
  // Similarly, when both are true, focus is changing hands within the dropdown
  // widget (and again, we should not do anything). We therefore only need to
  // look at when we gain initial focus and when we loose it.
  if (!our_view_before && our_view_now) {
    // We are gaining focus from outside the dropdown widget so we must register
    // a handler for Escape.
    RegisterEscAccelerator();
  } else if (our_view_before && !our_view_now) {
    // We are losing focus to something outside our widget so we restore the
    // original handler for Escape.
    UnregisterEscAccelerator();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DropdownBarHost, AnimationDelegate implementation:

void DropdownBarHost::AnimationProgressed(const Animation* animation) {
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
  view_->set_animation_offset(animation_offset_);
  view_->SchedulePaint();
}

void DropdownBarHost::AnimationEnded(const Animation* animation) {
  // Place the dropdown widget in its fully opened state.
  animation_offset_ = 0;

  if (!animation_->IsShowing()) {
    // Animation has finished closing.
    host_->Hide();
    is_visible_ = false;
  } else {
    // Animation has finished opening.
  }
}

void DropdownBarHost::GetThemePosition(gfx::Rect* bounds) {
  *bounds = GetDialogPosition(gfx::Rect());
  gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
  gfx::Rect tab_strip_bounds = browser_view_->GetTabStripBounds();
  bounds->Offset(-toolbar_bounds.x(), -tab_strip_bounds.y());
}

////////////////////////////////////////////////////////////////////////////////
// DropdownBarHost protected:

void DropdownBarHost::ResetFocusTracker() {
  focus_tracker_.reset(NULL);
}

void DropdownBarHost::GetWidgetBounds(gfx::Rect* bounds) {
  DCHECK(bounds);
  // The BrowserView does Layout for the components that we care about
  // positioning relative to, so we ask it to tell us where we should go.
  *bounds = browser_view_->GetFindBarBoundingBox();
}

void DropdownBarHost::RegisterEscAccelerator() {
  DCHECK(!esc_accel_target_registered_);
  views::Accelerator escape(base::VKEY_ESCAPE, false, false, false);
  focus_manager_->RegisterAccelerator(escape, this);
  esc_accel_target_registered_ = true;
}

void DropdownBarHost::UnregisterEscAccelerator() {
  DCHECK(esc_accel_target_registered_);
  views::Accelerator escape(base::VKEY_ESCAPE, false, false, false);
  focus_manager_->UnregisterAccelerator(escape, this);
  esc_accel_target_registered_ = false;
}
