// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credentials_item_view.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"

namespace {
// The default spacing between the icon and text.
const int kSpacing = 12;

gfx::Size GetTextLabelsSize(const views::Label* upper_label,
                            const views::Label* lower_label) {
  gfx::Size upper_label_size = upper_label ? upper_label->GetPreferredSize()
                                           : gfx::Size();
  gfx::Size lower_label_size = lower_label ? lower_label->GetPreferredSize()
                                           : gfx::Size();
  return gfx::Size(std::max(upper_label_size.width(), lower_label_size.width()),
                   upper_label_size.height() + lower_label_size.height());
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

CredentialsItemView::CredentialsItemView(
    views::ButtonListener* button_listener,
    const base::string16& upper_text,
    const base::string16& lower_text,
    SkColor hover_color,
    const autofill::PasswordForm* form,
    net::URLRequestContextGetter* request_context)
    : LabelButton(button_listener, base::string16()),
      form_(form),
      upper_label_(nullptr),
      lower_label_(nullptr),
      hover_color_(hover_color),
      weak_ptr_factory_(this) {
  set_notify_enter_exit_on_child(true);
  // Create an image-view for the avatar. Make sure it ignores events so that
  // the parent can receive the events instead.
  image_view_ = new CircularImageView;
  image_view_->set_interactive(false);
  gfx::Image image = ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE);
  DCHECK(image.Width() >= kAvatarImageSize &&
         image.Height() >= kAvatarImageSize);
  UpdateAvatar(image.AsImageSkia());
  if (form_->icon_url.is_valid()) {
    // Fetch the actual avatar.
    AccountAvatarFetcher* fetcher = new AccountAvatarFetcher(
        form_->icon_url, weak_ptr_factory_.GetWeakPtr());
    fetcher->Start(request_context);
  }
  AddChildView(image_view_);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  if (!upper_text.empty()) {
    upper_label_ = new views::Label(
        upper_text, rb->GetFontList(ui::ResourceBundle::SmallFont));
    upper_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(upper_label_);
  }

  if (!lower_text.empty()) {
    lower_label_ = new views::Label(
        lower_text, rb->GetFontList(ui::ResourceBundle::SmallFont));
    lower_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(lower_label_);
  }

  SetFocusBehavior(FocusBehavior::ALWAYS);
}

CredentialsItemView::~CredentialsItemView() = default;

void CredentialsItemView::UpdateAvatar(const gfx::ImageSkia& image) {
  image_view_->SetImage(ScaleImageForAccountAvatar(image));
}

void CredentialsItemView::SetLowerLabelColor(SkColor color) {
  if (lower_label_)
    lower_label_->SetEnabledColor(color);
}

void CredentialsItemView::SetHoverColor(SkColor color) {
  hover_color_ = color;
}

int CredentialsItemView::GetPreferredHeight() const {
  return GetPreferredSize().height();
}

gfx::Size CredentialsItemView::GetPreferredSize() const {
  gfx::Size labels_size = GetTextLabelsSize(upper_label_, lower_label_);
  gfx::Size size = gfx::Size(kAvatarImageSize + labels_size.width(),
                             std::max(kAvatarImageSize, labels_size.height()));
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

  gfx::Size upper_size =
      upper_label_ ? upper_label_->GetPreferredSize() : gfx::Size();
  gfx::Size lower_size =
      lower_label_ ? lower_label_->GetPreferredSize() : gfx::Size();
  int y_offset = (child_area.height() -
      (upper_size.height() + lower_size.height())) / 2;
  gfx::Point label_origin(image_origin.x() + image_size.width() + kSpacing,
                          child_area.origin().y() + y_offset);
  if (upper_label_)
    upper_label_->SetBoundsRect(gfx::Rect(label_origin, upper_size));
  if (lower_label_) {
    label_origin.Offset(0, upper_size.height());
    lower_label_->SetBoundsRect(gfx::Rect(label_origin, lower_size));
  }
}

void CredentialsItemView::OnPaint(gfx::Canvas* canvas) {
  if (state() == STATE_PRESSED || state() == STATE_HOVERED)
    canvas->DrawColor(hover_color_);

  LabelButton::OnPaint(canvas);
}
