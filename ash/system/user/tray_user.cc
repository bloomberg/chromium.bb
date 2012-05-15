// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/tray_user.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

const int kUserInfoVerticalPadding = 10;

const int kUserIconSize = 27;

}  // namespace

namespace ash {
namespace internal {

namespace tray {

// A custom image view with rounded edges.
class RoundedImageView : public views::View {
 public:
  // Constructs a new rounded image view with rounded corners of radius
  // |corner_radius|.
  explicit RoundedImageView(int corner_radius) : corner_radius_(corner_radius) {
  }

  virtual ~RoundedImageView() {
  }

  // Set the bitmap that should be displayed from a pointer. The pointer
  // contents is copied in the receiver's bitmap.
  void SetImage(const SkBitmap& bm, const gfx::Size& size) {
    image_ = bm;
    image_size_ = size;

    // Try to get the best image quality for the avatar.
    resized_ = skia::ImageOperations::Resize(image_,
        skia::ImageOperations::RESIZE_BEST, size.width(), size.height());
    if (GetWidget() && visible()) {
      PreferredSizeChanged();
      SchedulePaint();
    }
  }

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(image_size_.width() + GetInsets().width(),
                     image_size_.height() + GetInsets().height());
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    View::OnPaint(canvas);
    gfx::Rect image_bounds(GetPreferredSize());
    image_bounds = gfx::Rect(size()).Center(image_bounds.size());
    image_bounds.Inset(GetInsets());
    const SkScalar kRadius = SkIntToScalar(corner_radius_);
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(image_bounds), kRadius, kRadius);

    SkPaint paint;
    SkShader* shader = SkShader::CreateBitmapShader(resized_,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    SkMatrix shader_matrix;
    shader_matrix.setTranslate(SkIntToScalar(image_bounds.x()),
                               SkIntToScalar(image_bounds.y()));
    shader->setLocalMatrix(shader_matrix);

    paint.setShader(shader);
    paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
    shader->unref();
    canvas->DrawPath(path, paint);
  }

 private:
  SkBitmap image_;
  SkBitmap resized_;
  gfx::Size image_size_;
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundedImageView);
};

class UserView : public views::View,
                 public views::ButtonListener {
 public:
  explicit UserView(ash::user::LoginStatus login)
      : login_(login),
        container_(NULL),
        user_info_(NULL),
        username_(NULL),
        email_(NULL),
        signout_(NULL) {
    CHECK(login_ != ash::user::LOGGED_IN_NONE);
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    bool guest = login_ == ash::user::LOGGED_IN_GUEST;
    bool kiosk = login_ == ash::user::LOGGED_IN_KIOSK;
    bool locked = login_ == ash::user::LOGGED_IN_LOCKED;

    container_ = new TrayPopupTextButtonContainer;
    container_->layout()->set_spread_blank_space(false);
    AddChildView(container_);

    if (!guest && !kiosk)
      AddUserInfo();

    // A user should not be able to modify logged in state when screen is
    // locked.
    if (!locked)
      AddButtonContainer();
  }

  virtual ~UserView() {}

  // Create container for buttons.
  void AddButtonContainer() {
    bool guest = login_ == ash::user::LOGGED_IN_GUEST;
    bool kiosk = login_ == ash::user::LOGGED_IN_KIOSK;

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    if (kiosk) {
      views::Label* label = new views::Label;
      label->SetText(
          bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_KIOSK_LABEL));
      label->set_border(views::Border::CreateEmptyBorder(
            0, kTrayPopupPaddingHorizontal, 0, 1));
      label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      container_->AddChildView(label);
    }

    TrayPopupTextButton* button =
        new TrayPopupTextButton(this, bundle.GetLocalizedString(
          guest ? IDS_ASH_STATUS_TRAY_EXIT_GUEST :
          kiosk ? IDS_ASH_STATUS_TRAY_EXIT_KIOSK :
                  IDS_ASH_STATUS_TRAY_SIGN_OUT));
    container_->AddTextButton(button);
    signout_ = button;
  }

 private:
  void AddUserInfo() {
    user_info_ = new views::View;
    user_info_->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, kTrayPopupPaddingHorizontal,
        kUserInfoVerticalPadding, kTrayPopupPaddingBetweenItems));

    RoundedImageView* image = new RoundedImageView(kTrayRoundedBorderRadius);
    image->SetImage(ash::Shell::GetInstance()->tray_delegate()->GetUserImage(),
        gfx::Size(kUserIconSize, kUserIconSize));
    user_info_->AddChildView(image);

    views::View* user = new views::View;
    user->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 5, 0));
    ash::SystemTrayDelegate* tray =
        ash::Shell::GetInstance()->tray_delegate();
    username_ = new views::Label(UTF8ToUTF16(tray->GetUserDisplayName()));
    username_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    user->AddChildView(username_);

    email_ = new views::Label(UTF8ToUTF16(tray->GetUserEmail()));
    email_->SetFont(username_->font().DeriveFont(-1));
    email_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    email_->SetEnabled(false);
    user->AddChildView(email_);

    user_info_->AddChildView(user);
    container_->AddChildView(user_info_);
  }

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    CHECK(sender == signout_);
    ash::SystemTrayDelegate* tray = ash::Shell::GetInstance()->tray_delegate();
    tray->SignOut();
  }

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size;
    if (user_info_)
      size = user_info_->GetPreferredSize();
    if (signout_) {
      gfx::Size signout_size = signout_->GetPreferredSize();
      size.set_height(std::max(size.height(), signout_size.height()));
      size.set_width(size.width() + signout_size.width() +
          kTrayPopupPaddingHorizontal * 2 + kTrayPopupPaddingBetweenItems);
    }
    return size;
  }

  virtual void Layout() OVERRIDE {
    views::View::Layout();
    if (bounds().IsEmpty())
      return;

    container_->SetBoundsRect(gfx::Rect(size()));
    if (signout_) {
      gfx::Rect signout_bounds(signout_->GetPreferredSize());
      signout_bounds = bounds().Center(signout_bounds.size());
      signout_bounds.set_x(width() - signout_bounds.width() -
          kTrayPopupPaddingHorizontal);
      signout_->SetBoundsRect(signout_bounds);

      gfx::Rect usercard_bounds(user_info_->GetPreferredSize());
      usercard_bounds.set_width(signout_bounds.x());
      user_info_->SetBoundsRect(usercard_bounds);
    } else {
      user_info_->SetBoundsRect(gfx::Rect(size()));
    }
  }

  user::LoginStatus login_;

  TrayPopupTextButtonContainer* container_;
  views::View* user_info_;
  views::Label* username_;
  views::Label* email_;

  views::Button* signout_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

}  // namespace tray

TrayUser::TrayUser()
    : user_(NULL),
      avatar_(NULL) {
}

TrayUser::~TrayUser() {
}

views::View* TrayUser::CreateTrayView(user::LoginStatus status) {
  CHECK(avatar_ == NULL);
  avatar_ = new tray::RoundedImageView(kTrayRoundedBorderRadius);
  avatar_->set_border(views::Border::CreateEmptyBorder(0, 6, 0, 0));
  UpdateAfterLoginStatusChange(status);
  return avatar_;
}

views::View* TrayUser::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return NULL;

  CHECK(user_ == NULL);
  user_ = new tray::UserView(status);
  return user_;
}

views::View* TrayUser::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayUser::DestroyTrayView() {
  avatar_ = NULL;
}

void TrayUser::DestroyDefaultView() {
  user_ = NULL;
}

void TrayUser::DestroyDetailedView() {
}

void TrayUser::UpdateAfterLoginStatusChange(user::LoginStatus status) {
  if (status != user::LOGGED_IN_NONE && status != user::LOGGED_IN_KIOSK &&
      status != user::LOGGED_IN_GUEST) {
    avatar_->SetImage(
        ash::Shell::GetInstance()->tray_delegate()->GetUserImage(),
        gfx::Size(kUserIconSize, kUserIconSize));
    avatar_->SetVisible(true);
  } else {
    avatar_->SetVisible(false);
  }
}

void TrayUser::OnUserUpdate() {
  avatar_->SetImage(
      ash::Shell::GetInstance()->tray_delegate()->GetUserImage(),
      gfx::Size(kUserIconSize, kUserIconSize));
}

}  // namespace internal
}  // namespace ash
