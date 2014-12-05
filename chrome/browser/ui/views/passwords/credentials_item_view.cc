// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credentials_item_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace {
const int kIconSize = 50;
// The default spacing between the icon and text.
const int kSpacing = 5;

gfx::Size GetTextLabelsSize(const views::Label* full_name_label,
                            const views::Label* username_label) {
  gfx::Size full_name = full_name_label->GetPreferredSize();
  gfx::Size username = username_label ? username_label->GetPreferredSize() :
                                         gfx::Size();
  return gfx::Size(std::max(full_name.width(), username.width()),
                   full_name.height() + username.height());
}

class CircularImageView : public views::ImageView {
 public:
  CircularImageView() = default;

 private:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;

  DISALLOW_COPY_AND_ASSIGN(CircularImageView);
};

void CircularImageView::OnPaint(gfx::Canvas* canvas) {
  // Display the avatar picture as a circle.
  gfx::Rect bounds(GetImageBounds());
  gfx::Path circular_mask;
  circular_mask.addCircle(
      SkIntToScalar(bounds.x() + bounds.right()) / 2,
      SkIntToScalar(bounds.y() + bounds.bottom()) / 2,
      SkIntToScalar(std::min(bounds.height(), bounds.width())) / 2);
  canvas->ClipPath(circular_mask, true);
  ImageView::OnPaint(canvas);
}

}  // namespace

CredentialsItemView::CredentialsItemView(views::ButtonListener* button_listener,
                                         const autofill::PasswordForm& form)
    : LabelButton(button_listener, base::string16()),
      form_(form) {
  set_notify_enter_exit_on_child(true);
  // Create an image-view for the avatar. Make sure it ignores events so that
  // the parent can receive the events instead.
  image_view_ = new CircularImageView;
  image_view_->set_interactive(false);

  // TODO(vasilii): temporary code below shows the built-in profile icon instead
  // of avatar.
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const gfx::Image& image = cache.GetAvatarIconOfProfileAtIndex(0);
  image_view_->SetImage(profiles::GetSizedAvatarIcon(
      image, true, kIconSize, kIconSize).ToImageSkia());
  AddChildView(image_view_);

  // Add a label to show the full name.
  // TODO(vasilii): temporarily the label shows username.
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  full_name_label_ = new views::Label(
      form_.username_value, rb->GetFontList(ui::ResourceBundle::BoldFont));
  full_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(full_name_label_);

  // Add a label to show the user name.
  username_label_ = new views::Label(
      form_.username_value, rb->GetFontList(ui::ResourceBundle::SmallFont));
  username_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  username_label_->SetEnabled(false);
  AddChildView(username_label_);

  SetFocusable(true);
}

CredentialsItemView::~CredentialsItemView() = default;

gfx::Size CredentialsItemView::GetPreferredSize() const {
  gfx::Size labels_size = GetTextLabelsSize(full_name_label_, username_label_);
  gfx::Size size = gfx::Size(kIconSize + labels_size.width(),
                             std::max(kIconSize, labels_size.height()));
  const gfx::Insets insets(GetInsets());
  size.Enlarge(insets.width(), insets.height());
  size.Enlarge(kSpacing, 0);

  // Make the size at least as large as the minimum size needed by the border.
  size.SetToMax(border() ? border()->GetMinimumSize() : gfx::Size());
  return size;
}

int CredentialsItemView::GetHeightForWidth(int w) const {
  return View::GetHeightForWidth(w);
}

void CredentialsItemView::Layout() {
  gfx::Rect child_area(GetChildAreaBounds());
  child_area.Inset(GetInsets());

  gfx::Size image_size(image_view_->GetPreferredSize());
  image_size.SetToMin(child_area.size());
  gfx::Point image_origin(child_area.origin());
  image_origin.Offset(0, (child_area.height() - image_size.height()) / 2);
  image_view_->SetBoundsRect(gfx::Rect(image_origin, image_size));

  gfx::Size full_name_size = full_name_label_->GetPreferredSize();
  gfx::Size username_size =
      username_label_ ? username_label_->GetPreferredSize() : gfx::Size();
  int y_offset = (child_area.height() -
      (full_name_size.height() + username_size.height())) / 2;
  gfx::Point label_origin(image_origin.x() + image_size.width() + kSpacing,
                          child_area.origin().y() + y_offset);
  full_name_label_->SetBoundsRect(gfx::Rect(label_origin, full_name_size));
  if (username_label_) {
    label_origin.Offset(0, full_name_size.height());
    username_label_->SetBoundsRect(gfx::Rect(label_origin, username_size));
  }
}
