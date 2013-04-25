// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class MockObject {
 public:
  MOCK_METHOD0(Callback, void(void));
};

class ProfileResetterTest : public testing::Test {
 public:
  ProfileResetterTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        resetter_(NULL) {}

  ~ProfileResetterTest() {}

 private:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

 protected:
  testing::StrictMock<MockObject> mock_object_;
  ProfileResetter resetter_;
};

TEST_F(ProfileResetterTest, ResettableFlagsSize) {
  // Fail in case we need to adapt the typedef of
  // ProfileResetter::ResettableFlags because the underlying type of
  // ProfileResetter::Resettable has changed.
  ASSERT_GE(ProfileResetter::ALL, 0);
  EXPECT_GE((unsigned long)
            std::numeric_limits<ProfileResetter::ResettableFlags>::max(),
            (unsigned long) ProfileResetter::ALL);
}

TEST_F(ProfileResetterTest, ResetDefaultSearchEngine) {
  EXPECT_CALL(mock_object_, Callback());
  resetter_.Reset(
      ProfileResetter::DEFAULT_SEARCH_ENGINE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::Callback, base::Unretained(&mock_object_)));
}

TEST_F(ProfileResetterTest, ResetHomepage) {
  EXPECT_CALL(mock_object_, Callback());
  resetter_.Reset(
      ProfileResetter::HOMEPAGE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::Callback, base::Unretained(&mock_object_)));
}

TEST_F(ProfileResetterTest, ResetContentSettings) {
  EXPECT_CALL(mock_object_, Callback());
  resetter_.Reset(
      ProfileResetter::CONTENT_SETTINGS,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::Callback, base::Unretained(&mock_object_)));
}

TEST_F(ProfileResetterTest, ResetCookiesAndSiteData) {
  EXPECT_CALL(mock_object_, Callback());
  resetter_.Reset(
      ProfileResetter::COOKIES_AND_SITE_DATA,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::Callback, base::Unretained(&mock_object_)));
}

TEST_F(ProfileResetterTest, ResetExtensionsByDisabling) {
  EXPECT_CALL(mock_object_, Callback());
  resetter_.Reset(
      ProfileResetter::EXTENSIONS,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::Callback, base::Unretained(&mock_object_)));
}

TEST_F(ProfileResetterTest, ResetExtensionsByUninstalling) {
  EXPECT_CALL(mock_object_, Callback());
  resetter_.Reset(
      ProfileResetter::EXTENSIONS,
      ProfileResetter::UNINSTALL_EXTENSIONS,
      base::Bind(&MockObject::Callback, base::Unretained(&mock_object_)));
}

TEST_F(ProfileResetterTest, ResetExtensionsAll) {
  // mock_object_ is a StrictMock, so we verify that it is called only once.
  EXPECT_CALL(mock_object_, Callback());
  resetter_.Reset(
      ProfileResetter::ALL,
      ProfileResetter::UNINSTALL_EXTENSIONS,
      base::Bind(&MockObject::Callback, base::Unretained(&mock_object_)));
}

}  // namespace
