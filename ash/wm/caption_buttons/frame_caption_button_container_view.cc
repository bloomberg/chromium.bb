// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/caption_buttons/frame_maximize_button.h"
#include "grit/ash_resources.h"
#include "grit/ui_strings.h"  // Accessibility names
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// The distance between buttons.
const int kDistanceBetweenButtons = -1;

// Converts |point| from |src| to |dst| and hittests against |dst|.
bool ConvertPointToViewAndHitTest(const views::View* src,
                                  const views::View* dst,
                                  const gfx::Point& point) {
  gfx::Point converted(point);
  views::View::ConvertPointToTarget(src, dst, &converted);
  return dst->HitTestPoint(converted);
}

}  // namespace

// static
const char FrameCaptionButtonContainerView::kViewClassName[] =
    "FrameCaptionButtonContainerView";

FrameCaptionButtonContainerView::FrameCaptionButtonContainerView(
    views::Widget* frame,
    MinimizeAllowed minimize_allowed)
    : frame_(frame),
      header_style_(HEADER_STYLE_SHORT),
      minimize_button_(NULL),
      size_button_(NULL),
      close_button_(NULL) {
  bool alternate_style = switches::UseAlternateFrameCaptionButtonStyle();

  // Insert the buttons left to right.
  minimize_button_ = new views::ImageButton(this);
  minimize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  // Hide |minimize_button_| when using the non-alternate button style because
  // |size_button_| is capable of minimizing in this case.
  minimize_button_->SetVisible(
      minimize_allowed == MINIMIZE_ALLOWED &&
      (alternate_style || !frame_->widget_delegate()->CanMaximize()));
  AddChildView(minimize_button_);

  if (alternate_style)
    size_button_ = new views::ImageButton(this);
  else
    size_button_ = new FrameMaximizeButton(this, frame);
  size_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
  size_button_->SetVisible(frame_->widget_delegate()->CanMaximize());
  AddChildView(size_button_);

  close_button_ = new views::ImageButton(this);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_);

  button_separator_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_WINDOW_BUTTON_SEPARATOR).AsImageSkia();
}

FrameCaptionButtonContainerView::~FrameCaptionButtonContainerView() {
}

FrameMaximizeButton*
FrameCaptionButtonContainerView::GetOldStyleSizeButton() {
  return switches::UseAlternateFrameCaptionButtonStyle() ?
      NULL : static_cast<FrameMaximizeButton*>(size_button_);
}

void FrameCaptionButtonContainerView::ResetWindowControls() {
  minimize_button_->SetState(views::CustomButton::STATE_NORMAL);
  size_button_->SetState(views::CustomButton::STATE_NORMAL);
}

int FrameCaptionButtonContainerView::NonClientHitTest(
    const gfx::Point& point) const {
  if (close_button_->visible() &&
      ConvertPointToViewAndHitTest(this, close_button_, point)) {
    return HTCLOSE;
  } else if (size_button_->visible() &&
             ConvertPointToViewAndHitTest(this, size_button_, point)) {
    return HTMAXBUTTON;
  } else if (minimize_button_->visible() &&
             ConvertPointToViewAndHitTest(this, minimize_button_, point)) {
    return HTMINBUTTON;
  }
  return HTNOWHERE;
}

gfx::Size FrameCaptionButtonContainerView::GetPreferredSize() {
  int width = 0;
  bool first_visible = true;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;

    width += child_at(i)->GetPreferredSize().width();
    if (!first_visible)
      width += kDistanceBetweenButtons;
    first_visible = false;
  }
  return gfx::Size(width, close_button_->GetPreferredSize().height());
}

void FrameCaptionButtonContainerView::Layout() {
  SetButtonImages(minimize_button_,
                  IDR_AURA_WINDOW_MINIMIZE_SHORT,
                  IDR_AURA_WINDOW_MINIMIZE_SHORT_H,
                  IDR_AURA_WINDOW_MINIMIZE_SHORT_P);
  if (header_style_ == HEADER_STYLE_SHORT) {
    // The new assets only make sense if the window is maximized or fullscreen
    // because we usually use a black header in this case.
    if (frame_->IsMaximized() || frame_->IsFullscreen()) {
      SetButtonImages(size_button_,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE2,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_H,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_P);
      SetButtonImages(close_button_,
                      IDR_AURA_WINDOW_MAXIMIZED_CLOSE2,
                      IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_H,
                      IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_P);
    } else {
      SetButtonImages(size_button_,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P);
      SetButtonImages(close_button_,
                      IDR_AURA_WINDOW_MAXIMIZED_CLOSE,
                      IDR_AURA_WINDOW_MAXIMIZED_CLOSE_H,
                      IDR_AURA_WINDOW_MAXIMIZED_CLOSE_P);
    }
  } else {
    SetButtonImages(size_button_,
                    IDR_AURA_WINDOW_MAXIMIZE,
                    IDR_AURA_WINDOW_MAXIMIZE_H,
                    IDR_AURA_WINDOW_MAXIMIZE_P);
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_CLOSE,
                    IDR_AURA_WINDOW_CLOSE_H,
                    IDR_AURA_WINDOW_CLOSE_P);
  }

  int x = 0;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;

    gfx::Size size = child->GetPreferredSize();
    child->SetBounds(x, 0, size.width(), size.height());
    x += size.width() + kDistanceBetweenButtons;
  }
}

const char* FrameCaptionButtonContainerView::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButtonContainerView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // The alternate button style does not paint the button separator.
  if (!switches::UseAlternateFrameCaptionButtonStyle()) {
    // We should have at most two visible buttons. The button separator is
    // always painted underneath the close button regardless of whether a
    // button other than the close button is visible.
    gfx::Rect divider(close_button_->bounds().origin(),
                      button_separator_.size());
    canvas->DrawImageInt(button_separator_,
                         GetMirroredXForRect(divider),
                         divider.y());
  }
}

void FrameCaptionButtonContainerView::ButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  // When shift-clicking, slow down animations for visual debugging.
  // We used to do this via an event filter that looked for the shift key being
  // pressed but this interfered with several normal keyboard shortcuts.
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> slow_duration_mode;
  if (event.IsShiftDown()) {
    slow_duration_mode.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  }

  ash::UserMetricsAction action =
      ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MINIMIZE;
  if (sender == minimize_button_) {
    // The minimize button may move out from under the cursor.
    ResetWindowControls();
    frame_->Minimize();
  } else if (sender == size_button_) {
    // The size button may move out from under the cursor.
    ResetWindowControls();
    if (frame_->IsFullscreen()) { // Can be clicked in immersive fullscreen.
      frame_->SetFullscreen(false);
      action = ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_EXIT_FULLSCREEN;
    } else if (frame_->IsMaximized()) {
      frame_->Restore();
      action = ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE;
    } else {
      frame_->Maximize();
      action = ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MAXIMIZE;
    }
  } else if(sender == close_button_) {
    frame_->Close();
    action = ash::UMA_WINDOW_CLOSE_BUTTON_CLICK;
  } else {
    return;
  }
  ash::Shell::GetInstance()->delegate()->RecordUserMetricsAction(action);
}

void FrameCaptionButtonContainerView::SetButtonImages(
    views::ImageButton* button,
    int normal_image_id,
    int hot_image_id,
    int pushed_image_id) {
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  button->SetImage(views::CustomButton::STATE_NORMAL,
                   resource_bundle.GetImageSkiaNamed(normal_image_id));
  button->SetImage(views::CustomButton::STATE_HOVERED,
                   resource_bundle.GetImageSkiaNamed(hot_image_id));
  button->SetImage(views::CustomButton::STATE_PRESSED,
                   resource_bundle.GetImageSkiaNamed(pushed_image_id));
}

}  // namespace ash
