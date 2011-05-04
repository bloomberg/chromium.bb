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
#include "views/controls/button/image_button.h"
#include "views/layout/grid_layout.h"

namespace chromeos {

namespace {

// Size of the default image within the view.
const int kDefaultImageSize = 64;
// Margin from left and right sides to the view contents.
const int kHorizontalMargin = 10;
// Margin from top and bottom sides to the view contents.
const int kVerticalMargin = 5;
// Margin around the default image where selection border is drawn.
const int kSelectionMargin = 5;
// Padding between image columns.
const int kHorizontalPadding = 10;
// Padding between image rows.
const int kVerticalPadding = 5;
// Number of columns in a row of default images.
const int kColumnsCount = 5;
// We give each image control an ID so we could distinguish them. Since 0 is
// the default ID we want an offset for IDs we set.
const int kImageStartId = 100;
// ID for image control for video capture.
const int kCaptureButtonId = 1000;

}  // namespace

class UserImageButton : public views::ImageButton {
 public:
  explicit UserImageButton(views::ButtonListener* listener);

  // Changes button state to selected or unselected.
  void SetSelected(bool is_selected);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void OnPaintBackground(gfx::Canvas* canvas);

 private:
  bool is_selected_;

  DISALLOW_COPY_AND_ASSIGN(UserImageButton);
};

UserImageButton::UserImageButton(views::ButtonListener* listener)
    : ImageButton(listener),
      is_selected_(false) {
  set_border(views::Border::CreateEmptyBorder(
      kSelectionMargin,
      kSelectionMargin,
      kSelectionMargin,
      kSelectionMargin));
  set_background(CreateRoundedBackground(kSelectionMargin,
                                         1,
                                         SK_ColorLTGRAY,
                                         SK_ColorDKGRAY));
  SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
}

void UserImageButton::SetSelected(bool is_selected) {
  if (is_selected_ != is_selected) {
    is_selected_ = is_selected;
    SchedulePaint();
  }
}

gfx::Size UserImageButton::GetPreferredSize() {
  gfx::Size size = views::ImageButton::GetPreferredSize();
  size.Enlarge(GetInsets().width(), GetInsets().height());
  return size;
}

void UserImageButton::OnPaintBackground(gfx::Canvas* canvas) {
  // TODO(avayvod): Either implement border highlight or remove it.
  //  if (is_selected_)
  //  views::View::OnPaintBackground(canvas);
}


DefaultImagesView::DefaultImagesView(Delegate* delegate)
    : selected_image_index_(-1),
      delegate_(delegate) {
}

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
    return selected_image_index_ - 1;
}

void DefaultImagesView::ClearSelection() {
  if (selected_image_index_ != -1) {
    default_images_[selected_image_index_]->SetSelected(false);
    selected_image_index_ = -1;
  }
}

gfx::Size DefaultImagesView::GetPreferredSize() {
  int image_size_with_margin = (kDefaultImageSize + 2 * kSelectionMargin);
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
    selected_image_index_ = image_index + 1;
    default_images_[selected_image_index_]->SetSelected(true);
    if (delegate_)
      delegate_->OnImageSelected(image_index % kDefaultImagesCount);
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

  size_t image_count = default_images_.size();
  int rows_count = (image_count + kColumnsCount - 1) / kColumnsCount;
  for (int row = 0; row < rows_count; ++row) {
    views::ColumnSet* column_set = layout->AddColumnSet(row);
    for (int column = 0; column < kColumnsCount; ++column) {
      if (column != 0)
        column_set->AddPaddingColumn(1, kHorizontalPadding);
      column_set->AddColumn(
          views::GridLayout::LEADING,
          views::GridLayout::LEADING,
          1,
          views::GridLayout::USE_PREF,
          0,
          0);
    }
  }
  size_t current_image = 0;
  for (int row = 0; row < rows_count; ++row) {
    if (current_image >= default_images_.size())
      break;
    if (row != 0)
      layout->AddPaddingRow(1, kVerticalPadding);
    layout->StartRow(0, row);
    for (int column = 0; column < kColumnsCount; ++column) {
      if (current_image >= default_images_.size())
        break;
      layout->AddView(default_images_[current_image]);
      ++current_image;
    }
  }
}

}  // namespace chromeos
