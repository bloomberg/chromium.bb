// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "grit/generated_resources.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/throbber.h"

namespace {

// Background color of the login status label and signout button.
const SkColor kSignoutBackgroundColor = 0xFF007700;

// Left margin to the "Active User" text.
const int kSignoutLeftOffset = 10;

// Padding between menu button and left right image corner.
const int kMenuButtonPadding = 4;

const int kIdRemove = 1;
const int kIdChangePhoto = 2;

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
    signout_button_->SetFocusable(true);

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

  views::Button* signout_button() { return signout_button_; }

 private:
  friend class UserView;

  views::Label* active_user_label_;
  views::TextButton* signout_button_;

  DISALLOW_COPY_AND_ASSIGN(SignoutView);
};

UserView::UserView(Delegate* delegate, bool is_login)
    : delegate_(delegate),
      signout_view_(NULL),
      image_view_(new views::ImageView()),
      throbber_(CreateDefaultSmoothedThrobber()),
      menu_button_(NULL) {
  DCHECK(delegate);
  if (!is_login)
    signout_view_ = new SignoutView(this);
  if (is_login)
    menu_button_ = new views::MenuButton(NULL, std::wstring(), this, true);

  Init();
}

void UserView::Init() {
  image_view_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
  if (throbber_) {
    int w = throbber_->GetPreferredSize().width();
    int h = throbber_->GetPreferredSize().height();
    throbber_->SetBounds(kUserImageSize / 2 - w / 2,
        kUserImageSize / 2 - h / 2 , w, h);
    // Throbber should be actually hidden while stopped so tooltip manager
    // doesn't find it.
    throbber_->SetVisible(false);
    image_view_->AddChildView(throbber_);
  }

  // UserView's layout never changes, so let's layout once here.
  image_view_->SetBounds(0, 0, kUserImageSize, kUserImageSize);
  AddChildView(image_view_);
  if (signout_view_) {
    signout_view_->SetBounds(0, kUserImageSize, kUserImageSize,
                             signout_view_->GetPreferredSize().height());
    AddChildView(signout_view_);
  }
  if (menu_button_) {
    int w = menu_button_->GetPreferredSize().width();
    int h = menu_button_->GetPreferredSize().height();
    menu_button_->SetBounds(kUserImageSize - w - kMenuButtonPadding,
                            kMenuButtonPadding, w, h);
    menu_button_->SetVisible(false);
    AddChildView(menu_button_);
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

void UserView::SetTooltipText(const std::wstring& text) {
  DCHECK(image_view_);
  image_view_->SetTooltipText(text);
}

void UserView::StartThrobber() {
  throbber_->SetVisible(true);
  throbber_->Start();
}

void UserView::StopThrobber() {
  throbber_->Stop();
  throbber_->SetVisible(false);
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

void UserView::ButtonPressed(views::Button* sender, const views::Event& event) {
  DCHECK(delegate_);
  if (sender->tag() == login::SIGN_OUT)
    delegate_->OnSignout();
}

void UserView::SetMenuVisible(bool flag) {
  DCHECK(menu_button_);
  menu_button_->SetVisible(flag);
}

void UserView::BuildMenu() {
  menu_model_.reset(new menus::SimpleMenuModel(this));
  menu_model_->AddItemWithStringId(kIdRemove, IDS_LOGIN_REMOVE);
  menu_model_->AddItemWithStringId(kIdChangePhoto, IDS_LOGIN_CHANGE_PHOTO);
  menu_.reset(new views::Menu2(menu_model_.get()));
}

void UserView::RunMenu(View* source, const gfx::Point& pt) {
  if (!menu_.get())
    BuildMenu();

  menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

bool UserView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool UserView::IsCommandIdEnabled(int command_id) const {
  // TODO(dpolukhin): implement and enable change photo.
  return command_id != kIdChangePhoto;
}

bool UserView::GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator) {
  return false;
}

void UserView::ExecuteCommand(int command_id) {
  switch (command_id) {
    case kIdRemove:
      delegate_->OnRemoveUser();
      break;

    case kIdChangePhoto:
      delegate_->OnChangePhoto();
      break;
  }
}

}  // namespace chromeos
