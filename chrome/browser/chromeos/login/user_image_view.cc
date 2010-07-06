// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_view.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"

namespace {

// Margin in pixels from the left and right borders of screen's contents.
const int kHorizontalMargin = 10;
// Margin in pixels from the top and bottom borders of screen's contents.
const int kVerticalMargin = 10;
// Padding between horizontally neighboring elements.
const int kHorizontalPadding = 10;
// Padding between vertically neighboring elements.
const int kVerticalPadding = 10;
// Size for button with video image.
const int kVideoImageSize = 256;

}  // namespace

namespace chromeos {

using login::kUserImageSize;

UserImageView::UserImageView(Delegate* delegate)
    : title_label_(NULL),
      ok_button_(NULL),
      cancel_button_(NULL),
      video_button_(NULL),
      selected_image_(NULL),
      delegate_(delegate),
      image_selected_(false) {
}

UserImageView::~UserImageView() {
}

void UserImageView::Init() {
  // Use rounded rect background.
  set_border(CreateWizardBorder(&BorderDefinition::kScreenBorder));
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));

  // Set up fonts.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font = rb.GetFont(ResourceBundle::MediumBoldFont);

  title_label_ = new views::Label();
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetFont(title_font);
  title_label_->SetMultiLine(true);
  AddChildView(title_label_);

  SkBitmap video_button_image =
      skia::ImageOperations::Resize(
          *ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_USER_IMAGE_NO_VIDEO),
          skia::ImageOperations::RESIZE_BOX,
          kVideoImageSize,
          kVideoImageSize);

  video_button_ = new views::ImageButton(this);
  video_button_->SetImage(views::CustomButton::BS_NORMAL, &video_button_image);
  AddChildView(video_button_);

  selected_image_ = new views::ImageView();
  selected_image_->SetImageSize(
      gfx::Size(kUserImageSize, kUserImageSize));
  selected_image_->SetImage(
      *ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_LOGIN_OTHER_USER));
  AddChildView(selected_image_);

  UpdateLocalizedStrings();
}

void UserImageView::RecreateNativeControls() {
  // There is no way to get native button preferred size after the button was
  // sized so delete and recreate the button on text update.
  delete ok_button_;
  ok_button_ = new views::NativeButton(this, std::wstring());
  AddChildView(ok_button_);
  ok_button_->SetEnabled(image_selected_);
  if (image_selected_)
    ok_button_->RequestFocus();

  delete cancel_button_;
  cancel_button_ = new views::NativeButton(this, std::wstring());
  AddChildView(cancel_button_);
  cancel_button_->SetEnabled(true);
}

void UserImageView::UpdateLocalizedStrings() {
  RecreateNativeControls();

  title_label_->SetText(l10n_util::GetString(IDS_USER_IMAGE_SCREEN_TITLE));
  ok_button_->SetLabel(l10n_util::GetString(IDS_OK));
  cancel_button_->SetLabel(l10n_util::GetString(IDS_CANCEL));
  selected_image_->SetTooltipText(
      l10n_util::GetString(IDS_USER_IMAGE_SELECTED_TOOLTIP));
}

void UserImageView::UpdateVideoFrame(const SkBitmap& frame) {
  last_frame_.reset(new SkBitmap(frame));
  SkBitmap video_button_image =
      skia::ImageOperations::Resize(
          *last_frame_,
          skia::ImageOperations::RESIZE_BOX,
          kVideoImageSize,
          kVideoImageSize);

  video_button_->SetImage(views::CustomButton::BS_NORMAL, &video_button_image);
  video_button_->SchedulePaint();
}

void UserImageView::OnVideoImageClicked() {
  // TODO(avayvod): Snapshot sound.
  if (!last_frame_.get())
    return;

  selected_image_->SetImage(
      skia::ImageOperations::Resize(
          *last_frame_,
          skia::ImageOperations::RESIZE_LANCZOS3,
          kUserImageSize,
          kUserImageSize));
  image_selected_ = true;
  ok_button_->SetEnabled(true);
  ok_button_->RequestFocus();
}

void UserImageView::LocaleChanged() {
  UpdateLocalizedStrings();
  Layout();
}

void UserImageView::Layout() {
  gfx::Insets insets = GetInsets();

  // Place title at the top.
  int title_x = insets.left() + kHorizontalMargin;
  int title_y = insets.top() + kVerticalMargin;
  int max_width = width() - insets.width() - kHorizontalMargin * 2;
  title_label_->SizeToFit(max_width);

  gfx::Size title_size = title_label_->GetPreferredSize();
  title_label_->SetBounds(title_x,
                          title_y,
                          std::min(max_width, title_size.width()),
                          title_size.height());

  // Put OK button at the right bottom corner.
  gfx::Size ok_size = ok_button_->GetPreferredSize();
  int ok_x = width() - insets.right() - kHorizontalMargin - ok_size.width();
  int ok_y = height() - insets.bottom() - kVerticalMargin - ok_size.height();
  ok_button_->SetBounds(ok_x, ok_y, ok_size.width(), ok_size.height());

  // Put Cancel button to the left from OK.
  gfx::Size cancel_size = cancel_button_->GetPreferredSize();
  int cancel_x = ok_x - kHorizontalPadding - cancel_size.width();
  int cancel_y = ok_y;  // Height should be the same for both buttons.
  cancel_button_->SetBounds(cancel_x,
                            cancel_y,
                            cancel_size.width(),
                            cancel_size.height());

  // The area between buttons and title is for images.
  int title_bottom = title_label_->y() + title_label_->height();
  gfx::Rect images_area(insets.left() + kHorizontalMargin,
                        title_bottom + kVerticalPadding,
                        max_width,
                        ok_button_->y() - title_bottom -
                            2 * kVerticalPadding);

  // Selected image is floating in the middle between top and height, near
  // the right border.
  gfx::Size selected_image_size = selected_image_->GetPreferredSize();
  int selected_image_x = images_area.right() - selected_image_size.width();
  int selected_image_y = images_area.y() +
      (images_area.height() - selected_image_size.height()) / 2;
  selected_image_->SetBounds(selected_image_x,
                             selected_image_y,
                             selected_image_size.width(),
                             selected_image_size.height());

  // Video capture image is on the left side of the area, top aligned with
  // selected image.
  int video_button_x = images_area.x();
  int video_button_y = selected_image_y;
  video_button_->SetBounds(video_button_x,
                           video_button_y,
                           kVideoImageSize,
                           kVideoImageSize);

  SchedulePaint();
}

gfx::Size UserImageView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void UserImageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  DCHECK(delegate_);
  if (sender == video_button_) {
    OnVideoImageClicked();
    return;
  }
  if (sender == ok_button_)
    delegate_->OnOK(selected_image_->GetImage());
  else if (sender == cancel_button_)
    delegate_->OnCancel();
  else
    NOTREACHED();
}

}  // namespace chromeos

