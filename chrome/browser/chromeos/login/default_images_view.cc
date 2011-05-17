// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/default_images_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "grit/theme_resources.h"
#include "grit/generated_resources.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/border.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/layout/grid_layout.h"

namespace chromeos {

namespace {

// Size of the default image within the view.
const int kDefaultImageSize = 64;
// Margin from left and right sides to the view contents.
const int kHorizontalMargin = 10;
// Margin from top and bottom sides to the view contents.
const int kVerticalMargin = 10;
// Padding between image columns.
const int kHorizontalPadding = 20;
// Padding between image rows.
const int kVerticalPadding = 15;
// Number of columns in a row of default images.
const int kColumnsCount = 5;
// Size of the border around default image.
const int kImageBorderSize = 1;
// Color of default image border.
const SkColor kImageBorderColor = SkColorSetARGB(38, 0, 0, 0);
// Color of default image background.
const SkColor kImageBackgroundColor = SK_ColorWHITE;
// We give each image control an ID so we could distinguish them. Since 0 is
// the default ID we want an offset for IDs we set.
const int kImageStartId = 100;
// ID for image control for video capture.
const int kCaptureButtonId = 1000;
// A number of the first buttons that don't correspond to any default image
// (i.e. button to take a photo).
const int kNonDefaultImageButtonsCount = 1;

}  // namespace

// Image button with border and background. Corrects view size by border
// insets as ImageButton ignores it.
class UserImageButton : public views::ImageButton {
 public:
  explicit UserImageButton(views::ButtonListener* listener);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

 private:
  DISALLOW_COPY_AND_ASSIGN(UserImageButton);
};

UserImageButton::UserImageButton(views::ButtonListener* listener)
    : ImageButton(listener) {
  set_border(
      views::Border::CreateSolidBorder(kImageBorderSize, kImageBorderColor));
  set_background(
      views::Background::CreateSolidBackground(kImageBackgroundColor));
  SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
}

gfx::Size UserImageButton::GetPreferredSize() {
  gfx::Size size = views::ImageButton::GetPreferredSize();
  size.Enlarge(GetInsets().width(), GetInsets().height());
  return size;
}


DefaultImagesView::DefaultImagesView(Delegate* delegate)
    : selected_image_index_(-1),
      delegate_(delegate) {
}

DefaultImagesView::~DefaultImagesView() {}

void DefaultImagesView::Init() {
  UserImageButton* capture_button = new UserImageButton(this);
  capture_button->SetID(kCaptureButtonId);
  capture_button->SetTooltipText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO)));
  InitButton(IDR_BUTTON_USER_IMAGE_TAKE_PHOTO, capture_button);
  default_images_.push_back(capture_button);
  for (int i = 0; i < kDefaultImagesCount; ++i) {
    UserImageButton* image_button = new UserImageButton(this);
    image_button->SetID(i + kImageStartId);
    InitButton(kDefaultImageResources[i], image_button);
    default_images_.push_back(image_button);
  }
  InitLayout();
}

int DefaultImagesView::GetDefaultImageIndex() const {
  if (selected_image_index_ == -1)
    return -1;
  else
    return selected_image_index_ - kNonDefaultImageButtonsCount;
}

void DefaultImagesView::SetDefaultImageIndex(int image_index) {
  selected_image_index_ = image_index + kNonDefaultImageButtonsCount;
  if (delegate_)
    delegate_->OnImageSelected(image_index % kDefaultImagesCount);
}

void DefaultImagesView::ClearSelection() {
  selected_image_index_ = -1;
}

gfx::Size DefaultImagesView::GetPreferredSize() {
  int image_size_with_margin = (kDefaultImageSize + 2 * kImageBorderSize);
  int width = kColumnsCount * image_size_with_margin +
              (kColumnsCount - 1) * kHorizontalPadding;
  size_t image_count = default_images_.size();
  int rows_count = (image_count + kColumnsCount - 1) / kColumnsCount;
  int height = rows_count * image_size_with_margin +
               (rows_count - 1) * kVerticalPadding;
  return gfx::Size(width + 2 * kHorizontalMargin,
                   height + 2 * kVerticalMargin);
}

void DefaultImagesView::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  if (selected_image_index_ != -1 &&
      default_images_[selected_image_index_] == sender)
    return;
  ClearSelection();

  if (sender->GetID() == kCaptureButtonId) {
    if (delegate_)
      delegate_->OnCaptureButtonClicked();
  } else {
    int image_index = sender->GetID() - kImageStartId;
    int images_count = static_cast<int>(default_images_.size());
    if (image_index < 0 || image_index >= images_count) {
      NOTREACHED() << "Got ButtonPressed event from a view with wrong id.";
      return;
    }
    SetDefaultImageIndex(image_index);
  }
}

void DefaultImagesView::InitButton(int resource_id,
                                   UserImageButton* button) const {
  const SkBitmap* original_image =
      ResourceBundle::GetSharedInstance().GetBitmapNamed(resource_id);
  SkBitmap resized_image = skia::ImageOperations::Resize(
      *original_image,
      skia::ImageOperations::RESIZE_BEST,
      kDefaultImageSize, kDefaultImageSize);
  button->SetImage(
      views::CustomButton::BS_NORMAL,
      &resized_image);
}

void DefaultImagesView::InitLayout() {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(gfx::Insets(kVerticalMargin,
                                kHorizontalMargin,
                                kVerticalMargin,
                                kHorizontalMargin));
  SetLayoutManager(layout);

  size_t current_image = 0;
  size_t image_count = default_images_.size();
  int rows_count = (image_count + kColumnsCount - 1) / kColumnsCount;
  for (int row = 0; row < rows_count; ++row) {
    views::ColumnSet* column_set = layout->AddColumnSet(row);
    for (int column = 0; column < kColumnsCount; ++column) {
      if (column != 0)
        column_set->AddPaddingColumn(1, kHorizontalPadding);
      if (current_image < image_count) {
        column_set->AddColumn(
            views::GridLayout::LEADING,
            views::GridLayout::LEADING,
            1,
            views::GridLayout::USE_PREF,
            0,
            0);
      } else {
        int placeholders_count = kColumnsCount - column;
        int placeholders_width = placeholders_count *
                                 (kDefaultImageSize + 2 * kImageBorderSize);
        int padding_width = (placeholders_count - 1) * kHorizontalPadding;
        column_set->AddPaddingColumn(1, placeholders_width + padding_width);
        break;
      }
      ++current_image;
    }
  }
  current_image = 0;
  for (int row = 0; row < rows_count; ++row) {
    if (row != 0)
      layout->AddPaddingRow(1, kVerticalPadding);
    layout->StartRow(0, row);
    for (int column = 0; column < kColumnsCount; ++column) {
      if (current_image >= image_count)
        break;
      layout->AddView(default_images_[current_image]);
      ++current_image;
    }
  }
}

}  // namespace chromeos
