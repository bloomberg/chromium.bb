// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/tray_user.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

// A custom textbutton with some extra vertical padding, and custom border,
// alignment and hover-effects.
class TrayButton : public views::TextButton {
 public:
  TrayButton(views::ButtonListener* listener, const string16& text)
      : views::TextButton(listener, text),
        hover_(false),
        hover_bg_(views::Background::CreateSolidBackground(SkColorSetARGB(
               10, 0, 0, 0))) {
    set_alignment(ALIGN_CENTER);
  }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = views::TextButton::GetPreferredSize();
    size.Enlarge(0, 16);
    return size;
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    hover_ = true;
    SchedulePaint();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    hover_ = false;
    SchedulePaint();
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    if (hover_)
      hover_bg_->Paint(canvas, this);
    else
      views::TextButton::OnPaintBackground(canvas);
  }

  bool hover_;
  views::Background* hover_bg_;

  DISALLOW_COPY_AND_ASSIGN(TrayButton);
};

class UserView : public views::View,
                 public views::ButtonListener {
 public:
  explicit UserView(ash::user::LoginStatus status)
      : username_(NULL),
        email_(NULL),
        shutdown_(NULL),
        signout_(NULL),
        lock_(NULL) {
    CHECK(status != ash::user::LOGGED_IN_NONE);
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
          0, 0, 3));

    if (status != ash::user::LOGGED_IN_GUEST) {
      views::View* user = new views::View;
      user->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
            14, 5, 0));
      ash::SystemTrayDelegate* tray =
          ash::Shell::GetInstance()->tray_delegate();
      username_ = new views::Label(ASCIIToUTF16(tray->GetUserDisplayName()));
      username_->SetFont(username_->font().DeriveFont(2));
      username_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      user->AddChildView(username_);

      email_ = new views::Label(ASCIIToUTF16(tray->GetUserEmail()));
      email_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      email_->SetEnabled(false);
      user->AddChildView(email_);

      AddChildView(user);
    }

    views::View* button_container = new views::View;
    views::BoxLayout *layout = new
        views::BoxLayout(views::BoxLayout::kHorizontal, 0, 5, 0);
    layout->set_spread_blank_space(true);
    button_container->SetLayoutManager(layout);

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    shutdown_ = new TrayButton(this, bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_SHUT_DOWN));
    shutdown_->set_border(NULL);
    button_container->AddChildView(shutdown_);

    signout_ = new TrayButton(this, bundle.GetLocalizedString(
        status == ash::user::LOGGED_IN_GUEST ? IDS_ASH_STATUS_TRAY_EXIT_GUEST :
                                               IDS_ASH_STATUS_TRAY_SIGN_OUT));
    signout_->set_border(views::Border::CreateSolidSidedBorder(
          0, 1, 0, 1, SkColorSetARGB(25, 0, 0, 0)));
    button_container->AddChildView(signout_);

    if (status != ash::user::LOGGED_IN_GUEST) {
      lock_ = new TrayButton(this, bundle.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_LOCK));
      button_container->AddChildView(lock_);
      lock_->set_border(NULL);
    }

    AddChildView(button_container);
  }

 private:
  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    ash::SystemTrayDelegate* tray = ash::Shell::GetInstance()->tray_delegate();
    if (sender == shutdown_)
      tray->ShutDown();
    else if (sender == signout_)
      tray->SignOut();
    else if (sender == lock_)
      tray->LockScreen();
  }

  views::Label* username_;
  views::Label* email_;

  TrayButton* shutdown_;
  TrayButton* signout_;
  TrayButton* lock_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

}  // namespace

namespace ash {
namespace internal {

TrayUser::TrayUser() {
}

TrayUser::~TrayUser() {
}

views::View* TrayUser::CreateTrayView(user::LoginStatus status) {
  views::ImageView* avatar = new views::ImageView;
  if (status == user::LOGGED_IN_USER || status == user::LOGGED_IN_OWNER) {
    avatar->SetImage(
        ash::Shell::GetInstance()->tray_delegate()->GetUserImage());
    avatar->SetImageSize(gfx::Size(32, 32));
  }
  return avatar;
}

views::View* TrayUser::CreateDefaultView(user::LoginStatus status) {
  return status == user::LOGGED_IN_NONE ? NULL : new UserView(status);
}

views::View* TrayUser::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayUser::DestroyTrayView() {
}

void TrayUser::DestroyDefaultView() {
}

void TrayUser::DestroyDetailedView() {
}

}  // namespace internal
}  // namespace ash
