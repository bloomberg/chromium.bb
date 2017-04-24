// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extension_tab_util_delegate_chromeos.h"

#include <string>

#include "chromeos/login/login_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kTestUrl[] = "http://www.foo.bar/baz?key=val";
const char kFilteredUrl[] = "http://www.foo.bar/";

}  // namespace

class ExtensionTabUtilDelegateChromeOSTest : public testing::Test {
 protected:
  void SetUp() override;

  ExtensionTabUtilDelegateChromeOS delegate_;
  api::tabs::Tab tab_;
};

void ExtensionTabUtilDelegateChromeOSTest::SetUp() {
  tab_.url.reset(new std::string(kTestUrl));
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest, NoFilteringOutsidePublicSession) {
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());

  delegate_.ScrubTabForExtension(nullptr, nullptr, &tab_);
  EXPECT_EQ(kTestUrl, *tab_.url);
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest, ScrubURL) {
  // Set Public Session state.
  chromeos::LoginState::Initialize();
  chromeos::LoginState::Get()->SetLoggedInState(
    chromeos::LoginState::LOGGED_IN_ACTIVE,
    chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);

  delegate_.ScrubTabForExtension(nullptr, nullptr, &tab_);
  EXPECT_EQ(kFilteredUrl, *tab_.url);

  // Reset state at the end of test.
  chromeos::LoginState::Shutdown();
}

}  // namespace extensions
