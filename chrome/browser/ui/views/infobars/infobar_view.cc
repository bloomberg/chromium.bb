// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/focus/external_focus_tracker.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/hwnd_util.h"
#endif

// static
const double InfoBarView::kDefaultTargetHeight = 36.0;
const int InfoBarView::kHorizontalPadding = 6;
const int InfoBarView::kIconLabelSpacing = 6;
const int InfoBarView::kButtonButtonSpacing = 10;
const int InfoBarView::kEndOfLabelSpacing = 16;
const int InfoBarView::kCloseButtonSpacing = 12;
const int InfoBarView::kButtonInLabelSpacing = 5;

// InfoBarView, public: -------------------------------------------------------

InfoBarView::InfoBarView(InfoBarDelegate* delegate)
    : InfoBar(delegate),
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          close_button_(new views::ImageButton(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(delete_factory_(this)),
      target_height_(kDefaultTargetHeight) {
  // We delete ourselves when we're removed from the view hierarchy.
  set_parent_owned(false);

  set_background(new InfoBarBackground(delegate->GetInfoBarType()));

  switch (delegate->GetInfoBarType()) {
    case InfoBarDelegate::WARNING_TYPE:
      SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_WARNING));
      break;
    case InfoBarDelegate::PAGE_ACTION_TYPE:
      SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_PAGE_ACTION));
      break;
    default:
      NOTREACHED();
      break;
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::LINEAR);
}

InfoBarView::~InfoBarView() {
}

// InfoBarView, views::View overrides: ----------------------------------------

AccessibilityTypes::Role InfoBarView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_ALERT;
}

gfx::Size InfoBarView::GetPreferredSize() {
  int height = static_cast<int>(target_height_ * animation_->GetCurrentValue());
  return gfx::Size(0, height);
}

void InfoBarView::Layout() {
  gfx::Size button_ps = close_button_->GetPreferredSize();
  close_button_->SetBounds(width() - kHorizontalPadding - button_ps.width(),
                           OffsetY(this, button_ps), button_ps.width(),
                           button_ps.height());
}

void InfoBarView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  if (child == this) {
    if (is_add) {
      InfoBarAdded();
    } else {
      InfoBarRemoved();
    }
  }

  if (GetWidget() && GetWidget()->IsAccessibleWidget()) {
    // For accessibility, make the close button the last child view.
    if (parent == this && child != close_button_ &&
        HasChildView(close_button_) &&
        GetChildViewAt(GetChildViewCount() - 1) != close_button_) {
      RemoveChildView(close_button_);
      AddChildView(close_button_);
    }

    // Allow screen reader users to focus the close button.
    close_button_->SetFocusable(true);
  }
}

// InfoBarView, protected: ----------------------------------------------------

int InfoBarView::GetAvailableWidth() const {
  return close_button_->x() - kCloseButtonSpacing;
}

void InfoBarView::RemoveInfoBar() const {
  if (container_)
    container_->RemoveDelegate(delegate());
}

int InfoBarView::CenterY(const gfx::Size prefsize) {
  return std::max((static_cast<int>(target_height_) -
      prefsize.height()) / 2, 0);
}

int InfoBarView::OffsetY(views::View* parent, const gfx::Size prefsize) {
  return CenterY(prefsize) -
      (static_cast<int>(target_height_) - parent->height());
}

// InfoBarView, views::ButtonListener implementation: -------------------------

void InfoBarView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  if (sender == close_button_) {
    if (delegate_)
      delegate_->InfoBarDismissed();
    RemoveInfoBar();
  }
}

// InfoBarView, views::FocusChangeListener implementation: --------------------

void InfoBarView::FocusWillChange(View* focused_before, View* focused_now) {
  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now &&
      !this->IsParentOf(focused_before) && this->IsParentOf(focused_now)) {
    NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);
  }
}

// InfoBarView, ui::AnimationDelegate implementation: -------------------------

void InfoBarView::AnimationProgressed(const ui::Animation* animation) {
  if (container_)
    container_->InfoBarAnimated(true);
}

void InfoBarView::AnimationEnded(const ui::Animation* animation) {
  if (container_) {
    container_->InfoBarAnimated(false);

    if (!animation_->IsShowing())
      Close();
  }
}

// InfoBarView, private: ------------------------------------------------------

void InfoBarView::AnimateOpen() {
  animation_->Show();
}

void InfoBarView::Open() {
  // Set the animation value to 1.0 so that GetPreferredSize() returns the right
  // size.
  animation_->Reset(1.0);
  if (container_)
    container_->InfoBarAnimated(false);
}

void InfoBarView::AnimateClose() {
  bool restore_focus = true;
#if defined(OS_WIN)
  // Do not restore focus (and active state with it) on Windows if some other
  // top-level window became active.
  if (GetWidget() &&
      !ui::DoesWindowBelongToActiveWindow(GetWidget()->GetNativeView())) {
    restore_focus = false;
  }
#endif  // defined(OS_WIN)
  DestroyFocusTracker(restore_focus);
  animation_->Hide();
}

void InfoBarView::Close() {
  GetParent()->RemoveChildView(this);
  // Note that we only tell the delegate we're closed here, and not when we're
  // simply destroyed (by virtue of a tab switch or being moved from window to
  // window), since this action can cause the delegate to destroy itself.
  if (delegate_) {
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
}

void InfoBarView::InfoBarAdded() {
  // The container_ pointer must be set before adding to the view hierarchy.
  DCHECK(container_);
#if defined(OS_WIN)
  // When we're added to a view hierarchy within a widget, we create an
  // external focus tracker to track what was focused in case we obtain
  // focus so that we can restore focus when we're removed.
  views::Widget* widget = GetWidget();
  if (widget) {
    focus_tracker_.reset(new views::ExternalFocusTracker(this,
                                                         GetFocusManager()));
  }
#endif

  if (GetFocusManager())
    GetFocusManager()->AddFocusChangeListener(this);

  NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);
}

void InfoBarView::InfoBarRemoved() {
  DestroyFocusTracker(false);
  // NULL our container_ pointer so that if Animation::Stop results in
  // AnimationEnded being called, we do not try and delete ourselves twice.
  container_ = NULL;
  animation_->Stop();
  // Finally, clean ourselves up when we're removed from the view hierarchy
  // since no-one refers to us now.
  MessageLoop::current()->PostTask(FROM_HERE,
      delete_factory_.NewRunnableMethod(&InfoBarView::DeleteSelf));

  if (GetFocusManager())
    GetFocusManager()->RemoveFocusChangeListener(this);
}

void InfoBarView::DestroyFocusTracker(bool restore_focus) {
  if (focus_tracker_.get()) {
    if (restore_focus)
      focus_tracker_->FocusLastFocusedExternalView();
    focus_tracker_->SetFocusManager(NULL);
    focus_tracker_.reset(NULL);
  }
}

void InfoBarView::DeleteSelf() {
  delete this;
}
