// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "gfx/canvas_skia_paint.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
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
const int InfoBarView::kDefaultTargetHeight = 36;
const int InfoBarView::kHorizontalPadding = 6;
const int InfoBarView::kIconLabelSpacing = 6;
const int InfoBarView::kButtonButtonSpacing = 10;
const int InfoBarView::kEndOfLabelSpacing = 16;

InfoBarView::InfoBarView(InfoBarDelegate* delegate)
    : InfoBar(delegate),
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          close_button_(new views::ImageButton(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(delete_factory_(this)),
      target_height_(kDefaultTargetHeight) {
  set_parent_owned(false);  // InfoBar deletes itself at the appropriate time.

  InfoBarDelegate::Type infobar_type = delegate->GetInfoBarType();
  set_background(new InfoBarBackground(infobar_type));
  SetAccessibleName(l10n_util::GetStringUTF16(
      (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      IDS_ACCNAME_INFOBAR_WARNING : IDS_ACCNAME_INFOBAR_PAGE_ACTION));

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button_->SetFocusable(true);
  AddChildView(close_button_);

  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::LINEAR);
}

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
      !ui::DoesWindowBelongToActiveWindow(GetWidget()->GetNativeView()))
    restore_focus = false;
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

void InfoBarView::PaintArrow(gfx::Canvas* canvas,
                             View* outer_view,
                             int arrow_center_x) {
  gfx::Point infobar_top(0, y());
  ConvertPointToView(GetParent(), outer_view, &infobar_top);
  int infobar_top_y = infobar_top.y();
  SkPoint gradient_points[2] = {
      {SkIntToScalar(0), SkIntToScalar(infobar_top_y)},
      {SkIntToScalar(0), SkIntToScalar(infobar_top_y + target_height_)}
  };
  InfoBarDelegate::Type infobar_type = delegate_->GetInfoBarType();
  SkColor gradient_colors[2] = {
      InfoBarBackground::GetTopColor(infobar_type),
      InfoBarBackground::GetBottomColor(infobar_type),
  };
  SkShader* gradient_shader = SkGradientShader::CreateLinear(gradient_points,
      gradient_colors, NULL, 2, SkShader::kMirror_TileMode);
  SkPaint paint;
  paint.setStrokeWidth(1);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setShader(gradient_shader);
  gradient_shader->unref();

  // The size of the arrow (its height; also half its width).  The
  // arrow area is |arrow_size| ^ 2.  By taking the square root of the
  // animation value, we cause a linear animation of the area, which
  // matches the perception of the animation of the InfoBar.
  const int kArrowSize = 10;
  int arrow_size = static_cast<int>(kArrowSize *
                                    sqrt(animation_->GetCurrentValue()));
  SkPath fill_path;
  fill_path.moveTo(SkPoint::Make(SkIntToScalar(arrow_center_x - arrow_size),
                                 SkIntToScalar(infobar_top_y)));
  fill_path.rLineTo(SkIntToScalar(arrow_size), SkIntToScalar(-arrow_size));
  fill_path.rLineTo(SkIntToScalar(arrow_size), SkIntToScalar(arrow_size));
  SkPath border_path(fill_path);
  fill_path.close();
  gfx::CanvasSkia* canvas_skia = canvas->AsCanvasSkia();
  canvas_skia->drawPath(fill_path, paint);

  paint.setShader(NULL);
  paint.setColor(SkColorSetA(ResourceBundle::toolbar_separator_color,
                             SkColorGetA(gradient_colors[0])));
  paint.setStyle(SkPaint::kStroke_Style);
  canvas_skia->drawPath(border_path, paint);
}

InfoBarView::~InfoBarView() {
}

void InfoBarView::Layout() {
  gfx::Size button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(width() - kHorizontalPadding - button_size.width(),
                           OffsetY(this, button_size), button_size.width(),
                           button_size.height());
}

void InfoBarView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  View::ViewHierarchyChanged(is_add, parent, child);

  if (child == this) {
    if (is_add) {
      // The container_ pointer must be set before adding to the view hierarchy.
      DCHECK(container_);
#if defined(OS_WIN)
      // When we're added to a view hierarchy within a widget, we create an
      // external focus tracker to track what was focused in case we obtain
      // focus so that we can restore focus when we're removed.
      views::Widget* widget = GetWidget();
      if (widget) {
        focus_tracker_.reset(
            new views::ExternalFocusTracker(this, GetFocusManager()));
      }
#endif
      if (GetFocusManager())
        GetFocusManager()->AddFocusChangeListener(this);
      NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);
    } else {
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
  }

  // For accessibility, ensure the close button is the last child view.
  if ((parent == this) && (child != close_button_) &&
      HasChildView(close_button_) &&
      (GetChildViewAt(GetChildViewCount() - 1) != close_button_)) {
    RemoveChildView(close_button_);
    AddChildView(close_button_);
  }
}

void InfoBarView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  if (sender == close_button_) {
    if (delegate_)
      delegate_->InfoBarDismissed();
    RemoveInfoBar();
  }
}

void InfoBarView::AnimationProgressed(const ui::Animation* animation) {
  if (container_)
    container_->InfoBarAnimated(true);
}

int InfoBarView::GetAvailableWidth() const {
  const int kCloseButtonSpacing = 12;
  return close_button_->x() - kCloseButtonSpacing;
}

void InfoBarView::RemoveInfoBar() const {
  if (container_)
    container_->RemoveDelegate(delegate());
}

int InfoBarView::CenterY(const gfx::Size prefsize) const {
  return std::max((target_height_ - prefsize.height()) / 2, 0);
}

int InfoBarView::OffsetY(View* parent, const gfx::Size prefsize) const {
  return CenterY(prefsize) - (target_height_ - parent->height());
}

AccessibilityTypes::Role InfoBarView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_ALERT;
}

gfx::Size InfoBarView::GetPreferredSize() {
  return gfx::Size(0,
      static_cast<int>(target_height_ * animation_->GetCurrentValue()));
}

void InfoBarView::FocusWillChange(View* focused_before, View* focused_now) {
  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now && !this->IsParentOf(focused_before) &&
      this->IsParentOf(focused_now))
    NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);
}

void InfoBarView::AnimationEnded(const ui::Animation* animation) {
  if (container_) {
    container_->InfoBarAnimated(false);

    if (!animation_->IsShowing())
      Close();
  }
}

void InfoBarView::DestroyFocusTracker(bool restore_focus) {
  if (focus_tracker_ != NULL) {
    if (restore_focus)
      focus_tracker_->FocusLastFocusedExternalView();
    focus_tracker_->SetFocusManager(NULL);
    focus_tracker_.reset();
  }
}

void InfoBarView::DeleteSelf() {
  delete this;
}
