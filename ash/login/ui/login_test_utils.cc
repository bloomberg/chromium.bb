// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_test_utils.h"

namespace ash {

LockContentsView::TestApi MakeLockContentsViewTestApi(LockContentsView* view) {
  return LockContentsView::TestApi(view);
}

LoginAuthUserView::TestApi MakeLoginAuthUserViewTestApi(
    LockContentsView* view) {
  return LoginAuthUserView::TestApi(
      MakeLockContentsViewTestApi(view).auth_user_view());
}

LoginPasswordView::TestApi MakeLoginPasswordTestApi(LockContentsView* view) {
  return LoginPasswordView::TestApi(
      MakeLoginAuthUserViewTestApi(view).password_view());
}

}  // namespace ash
