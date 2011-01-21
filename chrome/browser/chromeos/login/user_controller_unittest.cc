// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

TEST(UserControllerTest, GetNameTooltip) {
  UserController guest_user_controller(NULL, false);
  EXPECT_EQ(UTF16ToWide(l10n_util::GetStringUTF16(IDS_ADD_USER)),
            guest_user_controller.GetNameTooltip());

  UserController new_user_controller(NULL, true);
  EXPECT_EQ(UTF16ToWide(l10n_util::GetStringUTF16(IDS_GO_INCOGNITO_BUTTON)),
            new_user_controller.GetNameTooltip());

  UserManager::User existing_user;
  existing_user.set_email("someordinaryuser@domain.com");
  UserController existing_user_controller(NULL, existing_user);
  EXPECT_EQ(L"someordinaryuser (domain.com)",
            existing_user_controller.GetNameTooltip());
}

}  // namespace chromeos
