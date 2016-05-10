// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_overlay.h"

#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Offset of the overlay from the edge of the work area.
const int kOffset = 5;

// Font style used for modifier key labels.
const ui::ResourceBundle::FontStyle kTextFontStyle =
    ui::ResourceBundle::MediumFont;

// Duration of slide animation when overlay is shown or hidden.
const int kSlideAnimationDurationMs = 100;

// Colors for the dismiss button.
const SkColor kButtonBackgroundColor = SkColorSetARGB(0xFF, 0x32, 0x32, 0x32);
const SkColor kButtonTextColor = SkColorSetARGB(0xFF, 0x7B, 0xAA, 0xF7);

// These values are in DIP.
const int kToastHorizontalSpacing = 16;
const int kToastVerticalSpacing = 16;
const int kToastChildSpacing = 32;
const int kToastMaximumWidth = 568;
const int kToastMinimumWidth = 288;

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlayLabel
class ToastOverlayLabel : public views::Label {
 public:
  explicit ToastOverlayLabel(const std::string& label);
  ~ToastOverlayLabel() override;

  void SetMaximumWidth(int width);

 private:
  int maximum_width_ = 0;

  gfx::Size GetPreferredSize() const override;

  DISALLOW_COPY_AND_ASSIGN(ToastOverlayLabel);
};

ToastOverlayLabel::ToastOverlayLabel(const std::string& label) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();

  SetText(base::UTF8ToUTF16(label));
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetFontList(rb->GetFontList(kTextFontStyle));
  SetAutoColorReadabilityEnabled(false);
  SetMultiLine(true);
  SetEnabledColor(SK_ColorWHITE);
  SetDisabledColor(SK_ColorWHITE);
  SetSubpixelRenderingEnabled(false);
}

ToastOverlayLabel::~ToastOverlayLabel() {}

void ToastOverlayLabel::SetMaximumWidth(int width) {
  maximum_width_ = width;
}

gfx::Size ToastOverlayLabel::GetPreferredSize() const {
  gfx::Size original_size = Label::GetPreferredSize();

  if (maximum_width_ != 0 && maximum_width_ < original_size.width())
    return gfx::Size(maximum_width_, GetHeightForWidth(maximum_width_));
  else
    return original_size;
}

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlayButton
class ToastOverlayButton : public views::LabelButton {
 public:
  explicit ToastOverlayButton(views::ButtonListener* listener,
                              const base::string16& label);
  ~ToastOverlayButton() override {}

 private:
  friend class ToastOverlay;  // for ToastOverlay::ClickDismissButtonForTesting.

  DISALLOW_COPY_AND_ASSIGN(ToastOverlayButton);
};

ToastOverlayButton::ToastOverlayButton(views::ButtonListener* listener,
                                       const base::string16& label)
    : views::LabelButton(listener, label) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();

  SetEnabledTextColors(kButtonTextColor);
  SetFontList(rb->GetFontList(kTextFontStyle));
  SetBorder(views::Border::NullBorder());
}

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlayView
class ToastOverlayView : public views::View, public views::ButtonListener {
 public:
  // This object is not owned by the views hiearchy or by the widget.
  ToastOverlayView(ToastOverlay* overlay, const std::string& text);
  ~ToastOverlayView() override;

  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  ToastOverlayButton* button() { return button_; }

 private:
  ToastOverlay* overlay_;       // weak
  ToastOverlayButton* button_;  // weak

  gfx::Size GetMaximumSize() const override;
  gfx::Size GetMinimumSize() const override;

  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  DISALLOW_COPY_AND_ASSIGN(ToastOverlayView);
};

ToastOverlayView::ToastOverlayView(ToastOverlay* overlay,
                                   const std::string& text)
    : overlay_(overlay),
      button_(new ToastOverlayButton(
          this,
          l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON))) {
  ToastOverlayLabel* label = new ToastOverlayLabel(text);
  label->SetMaximumWidth(GetMaximumSize().width() -
                         button_->GetPreferredSize().width() -
                         kToastHorizontalSpacing * 2 - kToastChildSpacing);
  AddChildView(label);

  AddChildView(button_);

  auto layout = new views::BoxLayout(views::BoxLayout::kHorizontal,
                                     kToastHorizontalSpacing,
                                     kToastVerticalSpacing, kToastChildSpacing);
  SetLayoutManager(layout);
  layout->SetFlexForView(label, 1);
  layout->SetFlexForView(button_, 0);
}

ToastOverlayView::~ToastOverlayView() {}

void ToastOverlayView::OnPaint(gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kButtonBackgroundColor);
  canvas->DrawRoundRect(GetLocalBounds(), 2, paint);
  views::View::OnPaint(canvas);
}

gfx::Size ToastOverlayView::GetMinimumSize() const {
  return gfx::Size(kToastMinimumWidth, 0);
}

gfx::Size ToastOverlayView::GetMaximumSize() const {
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForPrimaryDisplay()->shelf_layout_manager();
  gfx::Rect work_area_bounds = shelf_layout_manager->user_work_area_bounds();

  return gfx::Size(kToastMaximumWidth, work_area_bounds.height() - kOffset * 2);
}

void ToastOverlayView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  overlay_->Show(false);
}

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlay
ToastOverlay::ToastOverlay(Delegate* delegate, const std::string& text)
    : delegate_(delegate),
      text_(text),
      overlay_view_(new ToastOverlayView(this, text)),
      widget_size_(overlay_view_->GetPreferredSize()) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = true;
  params.keep_on_top = true;
  params.remove_standard_frame = true;
  params.bounds = CalculateOverlayBounds();
  // Show toasts above the app list and below the lock screen.
  params.parent = Shell::GetContainer(Shell::GetTargetRootWindow(),
                                      kShellWindowId_SystemModalContainer);
  overlay_widget_.reset(new views::Widget);
  overlay_widget_->Init(params);
  overlay_widget_->SetVisibilityChangedAnimationsEnabled(true);
  overlay_widget_->SetContentsView(overlay_view_.get());
  overlay_widget_->SetBounds(CalculateOverlayBounds());
  overlay_widget_->GetNativeView()->SetName("ToastOverlay");

  gfx::NativeWindow native_view = overlay_widget_->GetNativeView();
  ::wm::SetWindowVisibilityAnimationType(
      native_view, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationDuration(
      native_view,
      base::TimeDelta::FromMilliseconds(kSlideAnimationDurationMs));
}

ToastOverlay::~ToastOverlay() {
  overlay_widget_->Close();
}

void ToastOverlay::Show(bool visible) {
  if (overlay_widget_->GetLayer()->GetTargetVisibility() == visible)
    return;

  ui::LayerAnimator* animator = overlay_widget_->GetLayer()->GetAnimator();
  DCHECK(animator);

  base::TimeDelta original_duration = animator->GetTransitionDuration();
  ui::ScopedLayerAnimationSettings animation_settings(animator);
  // ScopedLayerAnimationSettings ctor chanes the transition duration, so change
  // back it to the original value (should be zero).
  animation_settings.SetTransitionDuration(original_duration);

  animation_settings.AddObserver(this);

  if (visible)
    overlay_widget_->Show();
  else
    overlay_widget_->Hide();
}

gfx::Rect ToastOverlay::CalculateOverlayBounds() {
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForPrimaryDisplay()->shelf_layout_manager();

  gfx::Rect bounds = shelf_layout_manager->user_work_area_bounds();
  int target_y = bounds.bottom() - widget_size_.height() - kOffset;
  bounds.ClampToCenteredSize(widget_size_);
  bounds.set_y(target_y);
  return bounds;
}

void ToastOverlay::OnImplicitAnimationsScheduled() {}

void ToastOverlay::OnImplicitAnimationsCompleted() {
  if (!overlay_widget_->GetLayer()->GetTargetVisibility())
    delegate_->OnClosed();
}

views::Widget* ToastOverlay::widget_for_testing() {
  return overlay_widget_.get();
}

void ToastOverlay::ClickDismissButtonForTesting(const ui::Event& event) {
  overlay_view_->button()->NotifyClick(event);
}

}  // namespace ash
