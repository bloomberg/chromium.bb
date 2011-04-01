// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/native_button.h"
#include "views/layout/grid_layout.h"

namespace {

// Margin in pixels from the left and right borders of screen's contents.
const int kHorizontalMargin = 10;
// Margin in pixels from the top and bottom borders of screen's contents.
const int kVerticalMargin = 10;
// Padding between horizontally neighboring elements.
const int kHorizontalPadding = 10;
// Padding between vertically neighboring elements.
const int kVerticalPadding = 10;

// IDs of column sets for grid layout manager.
enum ColumnSets {
  kTakePhotoRow, // Column set for image from camera and snapshot button.
  kButtonsRow,   // Column set for Skip and OK buttons.
};

}  // namespace

namespace chromeos {

UserImageView::UserImageView(Delegate* delegate)
    : take_photo_view_(NULL),
      ok_button_(NULL),
      skip_button_(NULL),
      delegate_(delegate) {
}

UserImageView::~UserImageView() {
}

void UserImageView::Init() {
  // Use rounded rect background.
  set_border(CreateWizardBorder(&BorderDefinition::kScreenBorder));
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));

  take_photo_view_ = new TakePhotoView(this);

  ok_button_ = new login::WideButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_OK)));
  ok_button_->SetEnabled(false);

  skip_button_ = new login::WideButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_SKIP)));
  skip_button_->SetEnabled(true);

  InitLayout();

  take_photo_view_->Init();
}

void UserImageView::InitLayout() {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(GetInsets());
  SetLayoutManager(layout);

  // The view with image from camera and label.
  views::ColumnSet* column_set = layout->AddColumnSet(kTakePhotoRow);
  column_set->AddPaddingColumn(0, kHorizontalMargin);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalMargin);

  // OK and Skip buttons are in the right bottom corner of the view.
  column_set = layout->AddColumnSet(kButtonsRow);
  column_set->AddPaddingColumn(0, kHorizontalMargin);
  // This column takes the empty space to the left of the buttons.
  column_set->AddPaddingColumn(1, 1);
  column_set->AddColumn(views::GridLayout::TRAILING,
                        views::GridLayout::TRAILING,
                        0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalPadding);
  column_set->AddColumn(views::GridLayout::TRAILING,
                        views::GridLayout::TRAILING,
                        0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalMargin);

  // Fill the layout with rows and views now.
  layout->StartRowWithPadding(0, kTakePhotoRow, 0, kVerticalMargin);
  layout->AddView(take_photo_view_);
  layout->StartRowWithPadding(0, kButtonsRow, 0, kVerticalPadding);
  layout->AddView(skip_button_);
  layout->AddView(ok_button_);
  layout->AddPaddingRow(0, kVerticalMargin);
}

void UserImageView::UpdateVideoFrame(const SkBitmap& frame) {
  DCHECK(take_photo_view_);
  take_photo_view_->UpdateVideoFrame(frame);
}

void UserImageView::ShowCameraInitializing() {
  DCHECK(take_photo_view_);
  take_photo_view_->ShowCameraInitializing();
}

void UserImageView::ShowCameraError() {
  DCHECK(take_photo_view_);
  take_photo_view_->ShowCameraError();
}

gfx::Size UserImageView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void UserImageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  DCHECK(delegate_);
  if (sender == ok_button_) {
    delegate_->OnOK(take_photo_view_->GetImage());
  } else if (sender == skip_button_) {
    delegate_->OnSkip();
  } else {
    NOTREACHED();
  }
}

void UserImageView::OnCapturingStarted() {
  ok_button_->SetEnabled(false);
}

void UserImageView::OnCapturingStopped() {
  ok_button_->SetEnabled(true);
  ok_button_->RequestFocus();
}

}  // namespace chromeos
