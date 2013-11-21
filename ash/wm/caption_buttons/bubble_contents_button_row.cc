// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/bubble_contents_button_row.h"

#include "ash/wm/caption_buttons/maximize_bubble_controller.h"
#include "ash/wm/caption_buttons/maximize_bubble_controller_bubble.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"


namespace ash {

// BubbleDialogButton ---------------------------------------------------------

// The image button gets overridden to be able to capture mouse hover events.
// The constructor also assigns all button states and adds |this| as a child of
// |button_row|.
class BubbleDialogButton : public views::ImageButton {
 public:
  explicit BubbleDialogButton(BubbleContentsButtonRow* button_row,
                              int normal_image,
                              int hovered_image,
                              int pressed_image);
  virtual ~BubbleDialogButton();

  // views::ImageButton:
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;

 private:
  // The creating class which needs to get notified in case of a hover event.
  BubbleContentsButtonRow* button_row_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDialogButton);
};

BubbleDialogButton::BubbleDialogButton(
    BubbleContentsButtonRow* button_row,
    int normal_image,
    int hovered_image,
    int pressed_image)
    : views::ImageButton(button_row),
      button_row_(button_row) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::CustomButton::STATE_NORMAL,
           rb.GetImageSkiaNamed(normal_image));
  SetImage(views::CustomButton::STATE_HOVERED,
           rb.GetImageSkiaNamed(hovered_image));
  SetImage(views::CustomButton::STATE_PRESSED,
           rb.GetImageSkiaNamed(pressed_image));
  button_row->AddChildView(this);
}

BubbleDialogButton::~BubbleDialogButton() {
}

void BubbleDialogButton::OnMouseCaptureLost() {
  button_row_->ButtonHovered(NULL);
  views::ImageButton::OnMouseCaptureLost();
}

void BubbleDialogButton::OnMouseEntered(const ui::MouseEvent& event) {
  button_row_->ButtonHovered(this);
  views::ImageButton::OnMouseEntered(event);
}

void BubbleDialogButton::OnMouseExited(const ui::MouseEvent& event) {
  button_row_->ButtonHovered(NULL);
  views::ImageButton::OnMouseExited(event);
}

bool BubbleDialogButton::OnMouseDragged(const ui::MouseEvent& event) {
  if (!button_row_->bubble()->controller())
    return false;

  // Remove the phantom window when we leave the button.
  gfx::Point screen_location(event.location());
  View::ConvertPointToScreen(this, &screen_location);
  if (!GetBoundsInScreen().Contains(screen_location))
    button_row_->ButtonHovered(NULL);
  else
    button_row_->ButtonHovered(this);

  // Pass the event on to the normal handler.
  return views::ImageButton::OnMouseDragged(event);
}


// BubbleContentsButtonRow ----------------------------------------------------

BubbleContentsButtonRow::BubbleContentsButtonRow(
    MaximizeBubbleControllerBubble* bubble)
    : bubble_(bubble),
      left_button_(NULL),
      minimize_button_(NULL),
      right_button_(NULL) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0,
      MaximizeBubbleControllerBubble::kLayoutSpacing));
  set_background(views::Background::CreateSolidBackground(
      MaximizeBubbleControllerBubble::kBubbleBackgroundColor));

  if (base::i18n::IsRTL()) {
    AddMaximizeRightButton();
    AddMinimizeButton();
    AddMaximizeLeftButton();
  } else {
    AddMaximizeLeftButton();
    AddMinimizeButton();
    AddMaximizeRightButton();
  }
}

BubbleContentsButtonRow::~BubbleContentsButtonRow() {
}

void BubbleContentsButtonRow::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  // While shutting down, the connection to the owner might already be broken.
  if (!bubble_->controller())
    return;
  if (sender == left_button_) {
    bubble_->controller()->OnButtonClicked(
        (bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_LEFT) ?
            SNAP_RESTORE : SNAP_LEFT);
  } else if (sender == minimize_button_) {
    bubble_->controller()->OnButtonClicked(SNAP_MINIMIZE);
  } else {
    DCHECK(sender == right_button_);
    bubble_->controller()->OnButtonClicked(
        (bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_RIGHT) ?
            SNAP_RESTORE : SNAP_RIGHT);
  }
}

void BubbleContentsButtonRow::ButtonHovered(BubbleDialogButton* sender) {
  // While shutting down, the connection to the owner might already be broken.
  if (!bubble_->controller())
    return;
  if (sender == left_button_) {
    bubble_->controller()->OnButtonHover(
        (bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_LEFT) ?
            SNAP_RESTORE : SNAP_LEFT);
  } else if (sender == minimize_button_) {
    bubble_->controller()->OnButtonHover(SNAP_MINIMIZE);
  } else if (sender == right_button_) {
    bubble_->controller()->OnButtonHover(
        (bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_RIGHT) ?
            SNAP_RESTORE : SNAP_RIGHT);
  } else {
    bubble_->controller()->OnButtonHover(SNAP_NONE);
  }
}

views::CustomButton* BubbleContentsButtonRow::GetButtonForUnitTest(
    SnapType state) {
  switch (state) {
    case SNAP_LEFT:
      return left_button_;
    case SNAP_MINIMIZE:
      return minimize_button_;
    case SNAP_RIGHT:
      return right_button_;
    default:
      NOTREACHED();
      return NULL;
  }
}

void BubbleContentsButtonRow::AddMaximizeLeftButton() {
  if (bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_LEFT) {
    left_button_ = new BubbleDialogButton(
        this,
        IDR_AURA_WINDOW_POSITION_LEFT_RESTORE,
        IDR_AURA_WINDOW_POSITION_LEFT_RESTORE_H,
        IDR_AURA_WINDOW_POSITION_LEFT_RESTORE_P);
  } else {
    left_button_ = new BubbleDialogButton(
        this,
        IDR_AURA_WINDOW_POSITION_LEFT,
        IDR_AURA_WINDOW_POSITION_LEFT_H,
        IDR_AURA_WINDOW_POSITION_LEFT_P);
  }
}

void BubbleContentsButtonRow::AddMaximizeRightButton() {
  if (bubble_->controller()->maximize_type() == FRAME_STATE_SNAP_RIGHT) {
    right_button_ = new BubbleDialogButton(
        this,
        IDR_AURA_WINDOW_POSITION_RIGHT_RESTORE,
        IDR_AURA_WINDOW_POSITION_RIGHT_RESTORE_H,
        IDR_AURA_WINDOW_POSITION_RIGHT_RESTORE_P);
  } else {
    right_button_ = new BubbleDialogButton(
        this,
        IDR_AURA_WINDOW_POSITION_RIGHT,
        IDR_AURA_WINDOW_POSITION_RIGHT_H,
        IDR_AURA_WINDOW_POSITION_RIGHT_P);
  }
}

void BubbleContentsButtonRow::AddMinimizeButton() {
  minimize_button_ = new BubbleDialogButton(
      this,
      IDR_AURA_WINDOW_POSITION_MIDDLE,
      IDR_AURA_WINDOW_POSITION_MIDDLE_H,
      IDR_AURA_WINDOW_POSITION_MIDDLE_P);
}

}  // namespace ash
