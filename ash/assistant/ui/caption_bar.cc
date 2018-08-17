// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/caption_bar.h"

#include <memory>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Appearance.
constexpr int kCaptionButtonSizeDip = 32;
constexpr int kPreferredHeightDip = 32;
constexpr int kVectorIconSizeDip = 12;

// CaptionButton ---------------------------------------------------------------

class CaptionButton : public views::ImageButton {
 public:
  explicit CaptionButton(const gfx::VectorIcon& icon,
                         views::ButtonListener* listener)
      : views::ImageButton(listener) {
    SetImage(
        views::Button::ButtonState::STATE_NORMAL,
        gfx::CreateVectorIcon(icon, kVectorIconSizeDip, gfx::kGoogleGrey700));
    SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
  }

  ~CaptionButton() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kCaptionButtonSizeDip, kCaptionButtonSizeDip);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptionButton);
};

}  // namespace

// CaptionBar ------------------------------------------------------------------

CaptionBar::CaptionBar() {
  InitLayout();
}

CaptionBar::~CaptionBar() = default;

gfx::Size CaptionBar::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int CaptionBar::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void CaptionBar::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Back.
  CaptionButton* back_button = new CaptionButton(kWindowControlBackIcon, this);
  back_button->set_id(static_cast<int>(CaptionButtonId::kBack));
  AddChildView(back_button);

  // Spacer.
  views::View* spacer = new views::View();
  AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  // Minimize.
  CaptionButton* minimize_button =
      new CaptionButton(kWindowControlMinimizeIcon, this);
  minimize_button->set_id(static_cast<int>(CaptionButtonId::kMinimize));
  AddChildView(minimize_button);

  // Close.
  CaptionButton* close_button =
      new CaptionButton(kWindowControlCloseIcon, this);
  close_button->set_id(static_cast<int>(CaptionButtonId::kClose));
  AddChildView(close_button);
}

void CaptionBar::ButtonPressed(views::Button* sender, const ui::Event& event) {
  CaptionButtonId id = static_cast<CaptionButtonId>(sender->id());

  // If the delegate returns |true| it has handled the event and wishes to
  // prevent default behavior from being performed.
  if (delegate_ && delegate_->OnCaptionButtonPressed(id))
    return;

  switch (id) {
    case CaptionButtonId::kClose:
      GetWidget()->Close();
      break;
    case CaptionButtonId::kBack:
    case CaptionButtonId::kMinimize:
      // No default behavior defined.
      NOTIMPLEMENTED();
      break;
  }
}

void CaptionBar::SetButtonVisible(CaptionButtonId id, bool visible) {
  views::View* button = GetViewByID(static_cast<int>(id));
  if (button)
    button->SetVisible(visible);
}

}  // namespace ash
