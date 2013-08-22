// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/frame_caption_button_container_view.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/workspace/frame_maximize_button.h"
#include "grit/ash_resources.h"
#include "grit/ui_strings.h"  // Accessibility names
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
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
    views::Widget* frame)
    : frame_(frame),
      size_button_(new FrameMaximizeButton(this, frame_view)),
      close_button_(new views::ImageButton(this)) {
  // Insert the buttons left to right.
  size_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
  if (ash::Shell::IsForcedMaximizeMode())
    size_button_->SetVisible(false);
  AddChildView(size_button_);

  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_);
}

FrameCaptionButtonContainerView::~FrameCaptionButtonContainerView() {
}

void FrameCaptionButtonContainerView::ResetWindowControls() {
  size_button_->SetState(views::CustomButton::STATE_NORMAL);
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
  SetButtonImages(size_button_,
                  IDR_AURA_WINDOW_FULLSCREEN_RESTORE,
                  IDR_AURA_WINDOW_FULLSCREEN_RESTORE_H,
                  IDR_AURA_WINDOW_FULLSCREEN_RESTORE_P);
  SetButtonImages(close_button_,
                  IDR_AURA_WINDOW_FULLSCREEN_CLOSE,
                  IDR_AURA_WINDOW_FULLSCREEN_CLOSE_H,
                  IDR_AURA_WINDOW_FULLSCREEN_CLOSE_P);

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
  ash::UserMetricsAction action =
      ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE;
  if (sender == size_button_) {
    // The maximize button may move out from under the cursor.
    ResetWindowControls();
    frame_->Restore();
  } else if (sender == close_button_) {
    action = ash::UMA_WINDOW_CLOSE_BUTTON_CLICK;
    frame_->Close();
  } else {
    return;
  }
  ash::Shell::GetInstance()->delegate()->RecordUserMetricsAction(action);
}

}  // namespace ash
