// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_test_utils.h"
#include "base/strings/string_split.h"

namespace ash {

LockContentsView::TestApi MakeLockContentsViewTestApi(LockContentsView* view) {
  return LockContentsView::TestApi(view);
}

LoginAuthUserView::TestApi MakeLoginPrimaryAuthTestApi(LockContentsView* view) {
  return LoginAuthUserView::TestApi(
      MakeLockContentsViewTestApi(view).primary_auth());
}

LoginPasswordView::TestApi MakeLoginPasswordTestApi(LockContentsView* view) {
  return LoginPasswordView::TestApi(
      MakeLoginPrimaryAuthTestApi(view).password_view());
}

mojom::LoginUserInfoPtr CreateUser(const std::string& email) {
  auto user = mojom::LoginUserInfo::New();
  user->basic_user_info = mojom::UserInfo::New();
  user->basic_user_info->account_id = AccountId::FromUserEmail(email);
  user->basic_user_info->display_name = base::SplitString(
      email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)[0];
  user->basic_user_info->display_email = email;
  return user;
}

}  // namespace ash
