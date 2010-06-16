// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "grit/generated_resources.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"

namespace {

// Background color of the login status label and signout button.
const SkColor kSignoutBackgroundColor = 0xFF007700;

// Left margin to the "Active User" text.
const int kSignoutLeftOffset = 10;

}  // namespace

namespace chromeos {

using login::kBackgroundColor;
using login::kBorderSize;
using login::kTextColor;
using login::kUserImageSize;

// The view that shows the Sign out button below the user's image.
class SignoutView : public views::View {
 public:
  explicit SignoutView(views::ButtonListener* listener) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::Font& font = rb.GetFont(ResourceBundle::SmallFont);

    active_user_label_ = new views::Label(
        l10n_util::GetString(IDS_SCREEN_LOCK_ACTIVE_USER));
    active_user_label_->SetFont(font);
    active_user_label_->SetColor(kTextColor);

    signout_button_ = new views::TextButton(
        listener, l10n_util::GetString(IDS_SCREEN_LOCK_SIGN_OUT));
    signout_button_->SetFont(font);
    signout_button_->SetEnabledColor(kTextColor);
    signout_button_->SetNormalHasBorder(false);
    signout_button_->set_tag(login::SIGN_OUT);

    AddChildView(active_user_label_);
    AddChildView(signout_button_);

    set_background(views::Background::CreateSolidBackground(
        kSignoutBackgroundColor));
  }

  // views::View overrides.
  virtual void Layout() {
    gfx::Size label = active_user_label_->GetPreferredSize();
    gfx::Size button = signout_button_->GetPreferredSize();
    active_user_label_->SetBounds(kSignoutLeftOffset,
                                  (height() - label.height()) / 2,
                                  label.width(), label.height());
    signout_button_->SetBounds(
        width() - button.width(), (height() - button.height()) / 2,
        button.width(), button.height());
  }

  virtual gfx::Size GetPreferredSize() {
    gfx::Size label = active_user_label_->GetPreferredSize();
    gfx::Size button = signout_button_->GetPreferredSize();
    return gfx::Size(label.width() + button.width(),
                     std::max(label.height(), button.height()));
  }

 private:
  friend class UserView;

  views::Label* active_user_label_;
  views::TextButton* signout_button_;

  DISALLOW_COPY_AND_ASSIGN(SignoutView);
};

UserView::UserView(views::ButtonListener* listener)
    : signout_view_(new SignoutView(listener)),
      image_view_(new views::ImageView()),
      throbber_(CreateDefaultSmoothedThrobber()) {
  DCHECK(listener);
  Init();
}

UserView::UserView()
    : signout_view_(NULL),
      image_view_(new views::ImageView()),
      throbber_(CreateDefaultSmoothedThrobber()) {
  Init();
}

void UserView::Init() {
  image_view_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
  int w = throbber_->GetPreferredSize().width();
  int h = throbber_->GetPreferredSize().height();
  throbber_->SetBounds(kUserImageSize / 2 - w / 2, kUserImageSize / 2 - h / 2 ,
                       w, h);
  image_view_->AddChildView(throbber_);

  // UserView's layout never changes, so let's layout once here.
  AddChildView(image_view_);
  image_view_->SetBounds(0, 0, kUserImageSize, kUserImageSize);
  if (signout_view_) {
    AddChildView(signout_view_);
    signout_view_->SetBounds(0, kUserImageSize, kUserImageSize,
                             signout_view_->GetPreferredSize().height());
  }
}

void UserView::SetImage(const SkBitmap& image) {
  int desired_size = std::min(image.width(), image.height());
  // Desired size is not preserved if it's greater than 75% of kUserImageSize.
  if (desired_size * 4 > 3 * kUserImageSize)
    desired_size = kUserImageSize;
  image_view_->SetImageSize(gfx::Size(desired_size, desired_size));
  image_view_->SetImage(image);
}

void UserView::StartThrobber() {
  throbber_->Start();
}

void UserView::StopThrobber() {
  throbber_->Stop();
}

gfx::Size UserView::GetPreferredSize() {
  return gfx::Size(
      kUserImageSize,
      kUserImageSize +
      (signout_view_ ? signout_view_->GetPreferredSize().height() : 0));
}

void UserView::SetSignoutEnabled(bool enabled) {
  DCHECK(signout_view_);
  signout_view_->signout_button_->SetEnabled(enabled);
}

}  // namespace chromeos
