// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/default_images_view.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
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
// Color for splitter separating contents from OK button.
const SkColor kSplitterColor = SkColorSetRGB(187, 195, 200);
// Height for the splitter.
const int kSplitterHeight = 1;

// IDs of column sets for grid layout manager.
enum ColumnSets {
  kTitleRow,     // Column set for screen title.
  kImagesRow,    // Column set for image from camera and snapshot button.
  kSplitterRow, // Place for the splitter.
  kButtonsRow,   // Column set for OK button.
};

views::View* CreateSplitter(const SkColor& color) {
  views::View* splitter = new views::View();
  splitter->set_background(views::Background::CreateSolidBackground(color));
  return splitter;
}

}  // namespace

namespace chromeos {

UserImageView::UserImageView(Delegate* delegate)
    : title_label_(NULL),
      default_images_view_(NULL),
      take_photo_view_(NULL),
      splitter_(NULL),
      ok_button_(NULL),
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

  title_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT)));
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetMultiLine(true);
  CorrectLabelFontSize(title_label_);

  default_images_view_ = new DefaultImagesView(this);
  take_photo_view_ = new TakePhotoView(this);
  take_photo_view_->set_show_title(false);

  splitter_ = CreateSplitter(kSplitterColor);

  ok_button_ = new login::WideButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_OK)));
  ok_button_->SetEnabled(false);

  InitLayout();

  default_images_view_->Init();
  take_photo_view_->Init();

  UserManager* user_manager = UserManager::Get();
  const std::string& logged_in_user = user_manager->logged_in_user().email();
  int image_index = user_manager->GetUserDefaultImageIndex(logged_in_user);

  default_images_view_->SetDefaultImageIndex(image_index);
}

void UserImageView::InitLayout() {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(GetInsets());
  SetLayoutManager(layout);

  // The title, left-top aligned.
  views::ColumnSet* column_set = layout->AddColumnSet(kTitleRow);
  column_set->AddPaddingColumn(0, kHorizontalMargin);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalMargin);

  // The view with default images and the one with image from camera and label.
  column_set = layout->AddColumnSet(kImagesRow);
  column_set->AddPaddingColumn(0, kHorizontalMargin);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalPadding);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalMargin);

  // Splitter.
  column_set = layout->AddColumnSet(kSplitterRow);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::TRAILING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);

  // OK button is in the right bottom corner of the view.
  column_set = layout->AddColumnSet(kButtonsRow);
  column_set->AddPaddingColumn(0, kHorizontalMargin);
  column_set->AddColumn(views::GridLayout::TRAILING,
                        views::GridLayout::TRAILING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalMargin);

  // Fill the layout with rows and views now.
  layout->StartRowWithPadding(0, kTitleRow, 0, kVerticalMargin);
  layout->AddView(title_label_);
  layout->StartRowWithPadding(0, kImagesRow, 0, kVerticalPadding);
  layout->AddView(default_images_view_);
  layout->AddView(take_photo_view_);
  layout->StartRowWithPadding(1, kSplitterRow, 0, kVerticalPadding);
  // Set height for splitter view explicitly otherwise it's set to 0
  // by default.
  layout->AddView(splitter_,
                  1, 1,
                  views::GridLayout::FILL,
                  views::GridLayout::TRAILING,
                  0,
                  kSplitterHeight);
  layout->StartRowWithPadding(0, kButtonsRow, 0, kVerticalPadding);
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

bool UserImageView::IsCapturing() const {
  DCHECK(default_images_view_);
  DCHECK(take_photo_view_);
  return default_images_view_->GetDefaultImageIndex() == -1 &&
         take_photo_view_->is_capturing();
}

gfx::Size UserImageView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void UserImageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  DCHECK(delegate_);
  if (sender == ok_button_) {
    if (default_images_view_->GetDefaultImageIndex() == -1) {
      delegate_->OnPhotoTaken(take_photo_view_->GetImage());
    } else {
      delegate_->OnDefaultImageSelected(
          default_images_view_->GetDefaultImageIndex());
    }
  } else {
    NOTREACHED();
  }
}

void UserImageView::OnCapturingStarted() {
  delegate_->StartCamera();
  default_images_view_->ClearSelection();
  ok_button_->SetEnabled(false);
  ShowCameraInitializing();
}

void UserImageView::OnCapturingStopped() {
  delegate_->StopCamera();
  default_images_view_->ClearSelection();
  ok_button_->SetEnabled(true);
  ok_button_->RequestFocus();
}

void UserImageView::OnCaptureButtonClicked() {
  if (!IsCapturing())
    OnCapturingStarted();
}

void UserImageView::OnImageSelected(int image_index) {
  // Can happen when user is not known, i.e. in tests.
  if (image_index < 0 || image_index >= kDefaultImagesCount)
    return;
  delegate_->StopCamera();
  ok_button_->SetEnabled(true);
  ok_button_->RequestFocus();
  take_photo_view_->SetImage(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
            kDefaultImageResources[image_index]));
}

}  // namespace chromeos
