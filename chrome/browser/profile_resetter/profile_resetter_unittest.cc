// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class MockObject {
 public:
  void RunLoop() {
    EXPECT_CALL(*this, Callback());
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

  void StopLoop() {
    DCHECK(runner_.get());
    Callback();
    runner_->Quit();
  }

 private:
  MOCK_METHOD0(Callback, void(void));

  scoped_refptr<content::MessageLoopRunner> runner_;
};

class ProfileResetterTest : public testing::Test {
 protected:
  ProfileResetterTest();
  ~ProfileResetterTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  testing::StrictMock<MockObject> mock_object_;
  TemplateURLServiceTestUtil test_util_;
  scoped_ptr<ProfileResetter> resetter_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetterTest);
};

ProfileResetterTest::ProfileResetterTest() {}

ProfileResetterTest::~ProfileResetterTest() {}

void ProfileResetterTest::SetUp() {
  test_util_.SetUp();
  resetter_.reset(new ProfileResetter(test_util_.profile()));
}

void ProfileResetterTest::TearDown() {
  test_util_.TearDown();
}

TEST_F(ProfileResetterTest, ResetDefaultSearchEngine) {
  resetter_->Reset(
      ProfileResetter::DEFAULT_SEARCH_ENGINE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

TEST_F(ProfileResetterTest, ResetHomepage) {
  PrefService* prefs = test_util_.profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, "http://google.com");
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  resetter_->Reset(
      ProfileResetter::HOMEPAGE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(std::string(), prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(ProfileResetterTest, ResetContentSettings) {
  resetter_->Reset(
      ProfileResetter::CONTENT_SETTINGS,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

TEST_F(ProfileResetterTest, ResetCookiesAndSiteData) {
  resetter_->Reset(
      ProfileResetter::COOKIES_AND_SITE_DATA,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

TEST_F(ProfileResetterTest, ResetExtensionsByDisabling) {
  resetter_->Reset(
      ProfileResetter::EXTENSIONS,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

TEST_F(ProfileResetterTest, ResetExtensionsByUninstalling) {
  resetter_->Reset(
      ProfileResetter::EXTENSIONS,
      ProfileResetter::UNINSTALL_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

TEST_F(ProfileResetterTest, ResetStartPage) {
  PrefService* prefs = test_util_.profile()->GetPrefs();
  DCHECK(prefs);

  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.push_back(GURL("http://foo"));
  startup_pref.urls.push_back(GURL("http://bar"));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  resetter_->Reset(
      ProfileResetter::STARTUP_PAGE,
      ProfileResetter::DISABLE_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(), startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(), startup_pref.urls);
}

TEST_F(ProfileResetterTest, ResetExtensionsAll) {
  // mock_object_ is a StrictMock, so we verify that it is called only once.
  resetter_->Reset(
      ProfileResetter::ALL,
      ProfileResetter::UNINSTALL_EXTENSIONS,
      base::Bind(&MockObject::StopLoop, base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

}  // namespace
