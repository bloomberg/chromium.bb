// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"

#include "ash/login/lock_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/shell.h"
#include "components/user_manager/user.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

const char* kLoginAuthUserViewClassName = "LoginAuthUserView";

// Distance between the username label and the password textfield.
const int kDistanceBetweenUsernameAndPasswordDp = 32;

}  // namespace

LoginAuthUserView::TestApi::TestApi(LoginAuthUserView* view) : view_(view) {}

LoginAuthUserView::TestApi::~TestApi() = default;

LoginPasswordView* LoginAuthUserView::TestApi::password_view() const {
  return view_->password_view_;
}

LoginAuthUserView::LoginAuthUserView(const mojom::UserInfoPtr& user,
                                     const OnAuthCallback& on_auth)
    : on_auth_(on_auth) {
  // Build child views.
  user_view_ =
      new LoginUserView(LoginDisplayStyle::kLarge, true /*show_dropdown*/);
  password_view_ = new LoginPasswordView(
      base::Bind(&LoginAuthUserView::OnAuthSubmit, base::Unretained(this),
                 false /*is_pin*/));
  pin_view_ =
      new LoginPinView(base::BindRepeating(&LoginPasswordView::AppendNumber,
                                           base::Unretained(password_view_)),
                       base::BindRepeating(&LoginPasswordView::Backspace,
                                           base::Unretained(password_view_)));

  // Build layout.
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        gfx::Insets(),
                                        kDistanceBetweenUsernameAndPasswordDp));

  // Note: |user_view_| will be sized to it's minimum size (not its preferred
  // size) because of the vertical box layout manager. This class expresses the
  // minimum preferred size again so everything works out as desired (ie, we can
  // control how far away the password auth is from the user label).
  AddChildView(user_view_);

  {
    // We need to center LoginPasswordAuth.
    //
    // Also, BoxLayout::kVertical will ignore preferred width, which messes up
    // separator rendering.
    auto* row = new views::View();
    AddChildView(row);

    auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    row->SetLayoutManager(layout);

    row->AddChildView(password_view_);
  }

  {
    // We need to center LoginPinAuth.
    auto* row = new views::View();
    AddChildView(row);

    auto* layout = new views::BoxLayout(views::BoxLayout::kHorizontal);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    row->SetLayoutManager(layout);

    row->AddChildView(pin_view_);
  }

  SetAuthMethods(auth_methods_);
  UpdateForUser(user);
}

LoginAuthUserView::~LoginAuthUserView() = default;

void LoginAuthUserView::SetAuthMethods(uint32_t auth_methods) {
  // TODO(jdufault): Implement additional auth methods.
  pin_view_->SetVisible((auth_methods & AUTH_PIN) != 0);
  PreferredSizeChanged();
}

void LoginAuthUserView::UpdateForUser(const mojom::UserInfoPtr& user) {
  current_user_ = user->account_id;
  user_view_->UpdateForUser(user);
}

const char* LoginAuthUserView::GetClassName() const {
  return kLoginAuthUserViewClassName;
}

gfx::Size LoginAuthUserView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  // Make sure we are at least as big as the user view. If we do not do this the
  // view will be below minimum size when no auth methods are displayed.
  size.SetToMax(user_view_->GetPreferredSize());
  return size;
}

void LoginAuthUserView::OnAuthSubmit(bool is_pin,
                                     const base::string16& password) {
  Shell::Get()->lock_screen_controller()->AuthenticateUser(
      current_user_, std::string(), is_pin,
      base::BindOnce([](OnAuthCallback on_auth,
                        bool auth_success) { on_auth.Run(auth_success); },
                     on_auth_));
}

}  // namespace ash
