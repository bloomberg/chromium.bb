// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/frame_caption_button_container_view.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/property_util.h"
#include "ash/wm/workspace/frame_maximize_button.h"
#include "grit/ash_resources.h"
#include "grit/ui_strings.h"  // Accessibility names
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

namespace {

// The space between the buttons.
const int kButtonGap = -1;

}  // namespace

// static
const char FrameCaptionButtonContainerView::kViewClassName[] =
    "FrameCaptionButtonContainerView";

FrameCaptionButtonContainerView::FrameCaptionButtonContainerView(
    views::NonClientFrameView* frame_view,
    views::Widget* frame,
    MinimizeAllowed minimize_allowed)
    : frame_(frame),
      header_style_(HEADER_STYLE_SHORT),
      minimize_button_(new views::ImageButton(this)),
      size_button_(new FrameMaximizeButton(this, frame_view)),
      close_button_(new views::ImageButton(this)) {
  // Insert the buttons left to right.
  minimize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  // Hide |minimize_button_| when |size_button_| is visible because
  // |size_button_| is capable of minimizing.
  // TODO(pkotwicz): We should probably show the minimize button when in
  // "always maximized" mode.
  minimize_button_->SetVisible(minimize_allowed == MINIMIZE_ALLOWED &&
                               !frame_->widget_delegate()->CanMaximize());
  AddChildView(minimize_button_);

  size_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
  size_button_->SetVisible(frame_->widget_delegate()->CanMaximize() &&
                           !ash::Shell::IsForcedMaximizeMode());
  AddChildView(size_button_);

  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_);

  button_separator_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_WINDOW_BUTTON_SEPARATOR).AsImageSkia();
}

FrameCaptionButtonContainerView::~FrameCaptionButtonContainerView() {
}

void FrameCaptionButtonContainerView::ResetWindowControls() {
  minimize_button_->SetState(views::CustomButton::STATE_NORMAL);
  size_button_->SetState(views::CustomButton::STATE_NORMAL);
}

int FrameCaptionButtonContainerView::NonClientHitTest(
    const gfx::Point& point) const {
  if (close_button_->visible() &&
      close_button_->GetMirroredBounds().Contains(point)) {
    return HTCLOSE;
  } else if (size_button_->visible() &&
             size_button_->GetMirroredBounds().Contains(point)) {
    return HTMAXBUTTON;
  } else if (minimize_button_->visible() &&
             minimize_button_->GetMirroredBounds().Contains(point)) {
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
      width += kButtonGap;
    first_visible = false;
  }
  gfx::Insets insets(GetInsets());
  return gfx::Size(
      width + insets.width(),
      close_button_->GetPreferredSize().height() + insets.height());
}

void FrameCaptionButtonContainerView::Layout() {
  SetButtonImages(minimize_button_,
                  IDR_AURA_WINDOW_MINIMIZE_SHORT,
                  IDR_AURA_WINDOW_MINIMIZE_SHORT_H,
                  IDR_AURA_WINDOW_MINIMIZE_SHORT_P);

  if (header_style_ == HEADER_STYLE_MAXIMIZED_HOSTED_APP) {
    SetButtonImages(size_button_,
                    IDR_AURA_WINDOW_FULLSCREEN_RESTORE,
                    IDR_AURA_WINDOW_FULLSCREEN_RESTORE_H,
                    IDR_AURA_WINDOW_FULLSCREEN_RESTORE_P);
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_FULLSCREEN_CLOSE,
                    IDR_AURA_WINDOW_FULLSCREEN_CLOSE_H,
                    IDR_AURA_WINDOW_FULLSCREEN_CLOSE_P);
  } else if (header_style_ == HEADER_STYLE_SHORT) {
    // The new assets only make sense if the window is maximized or fullscreen
    // because we usually use a black header in this case.
    if ((frame_->IsMaximized() || frame_->IsFullscreen()) &&
        GetTrackedByWorkspace(frame_->GetNativeWindow())) {
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

  gfx::Insets insets(GetInsets());
  int x = insets.left();
  int y_inset = insets.top();
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;

    gfx::Size size = child->GetPreferredSize();
    child->SetBounds(x, y_inset, size.width(), size.height());
    x += size.width() + kButtonGap;
  }
}

const char* FrameCaptionButtonContainerView::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButtonContainerView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // AppNonClientFrameViewAsh does not paint the button separator.
  if (header_style_ != HEADER_STYLE_MAXIMIZED_HOSTED_APP) {
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

}  // namespace ash
