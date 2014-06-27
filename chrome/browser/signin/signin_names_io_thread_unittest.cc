// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_names_io_thread.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class SigninNamesOnIOThreadTest : public testing::Test {
 public:
  SigninNamesOnIOThreadTest();
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void SimulateSignin(const base::string16& email);
  void AddNewProfile(const base::string16& name, const base::string16& email);

  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  FakeSigninManager* signin_manager_;
  TestingProfileManager testing_profile_manager_;
  SigninNamesOnIOThread signin_names_;
};

SigninNamesOnIOThreadTest::SigninNamesOnIOThreadTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_),
      io_thread_(content::BrowserThread::IO, &message_loop_),
      testing_profile_manager_(
          TestingBrowserProcess::GetGlobal()) {
}

void SigninNamesOnIOThreadTest::SetUp() {
  ASSERT_TRUE(testing_profile_manager_.SetUp());
  TestingProfile::Builder builder;
  builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                            FakeSigninManagerBase::Build);
  profile_ = builder.Build();
  signin_manager_ = static_cast<FakeSigninManager*>(
      SigninManagerFactory::GetForProfile(profile_.get()));
}

void SigninNamesOnIOThreadTest::TearDown() {
  signin_names_.ReleaseResourcesOnUIThread();
}

void SigninNamesOnIOThreadTest::SimulateSignin(const base::string16& email) {
  signin_manager_->SignIn(base::UTF16ToUTF8(email), "password");
}

void SigninNamesOnIOThreadTest::AddNewProfile(const base::string16& name,
                                              const base::string16& email) {
  ProfileInfoCache* cache = testing_profile_manager_.profile_info_cache();
  const base::FilePath& user_data_dir = cache->GetUserDataDir();
#if defined(OS_POSIX)
  const base::FilePath profile_dir =
      user_data_dir.Append(base::UTF16ToUTF8(name));
#else
  const base::FilePath profile_dir = user_data_dir.Append(name);
#endif
  cache->AddProfileToCache(profile_dir, name, email, 0, std::string());
}

TEST_F(SigninNamesOnIOThreadTest, Basic) {
  ASSERT_EQ(0u, signin_names_.GetEmails().size());
}

TEST_F(SigninNamesOnIOThreadTest, Signin) {
  const base::string16 email = base::UTF8ToUTF16("foo@gmail.com");
  SimulateSignin(email);

  const SigninNamesOnIOThread::EmailSet& emails = signin_names_.GetEmails();
  ASSERT_EQ(1u, emails.size());
  ASSERT_EQ(1u, emails.count(email));
}

TEST_F(SigninNamesOnIOThreadTest, Signout) {
  const base::string16 email = base::UTF8ToUTF16("foo@gmail.com");
  SimulateSignin(email);
  signin_manager_->SignOut(signin_metrics::SIGNOUT_TEST);

  const SigninNamesOnIOThread::EmailSet& emails = signin_names_.GetEmails();
  ASSERT_EQ(0u, emails.size());
}

TEST_F(SigninNamesOnIOThreadTest, StartWithConnectedProfiles) {
  // Setup a couple of connected profiles, and one unconnected.
  const base::string16 email1 = base::UTF8ToUTF16("foo@gmail.com");
  const base::string16 email2 = base::UTF8ToUTF16("bar@gmail.com");
  AddNewProfile(base::UTF8ToUTF16("foo"), email1);
  AddNewProfile(base::UTF8ToUTF16("bar"), email2);
  AddNewProfile(base::UTF8ToUTF16("toto"), base::string16());

  SigninNamesOnIOThread signin_names;

  const SigninNamesOnIOThread::EmailSet& emails = signin_names.GetEmails();
  ASSERT_EQ(2u, emails.size());
  ASSERT_EQ(1u, emails.count(email1));
  ASSERT_EQ(1u, emails.count(email2));

  signin_names.ReleaseResourcesOnUIThread();
}
