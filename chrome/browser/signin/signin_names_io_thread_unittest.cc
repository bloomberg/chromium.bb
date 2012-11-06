// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_names_io_thread.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestInternal : public SigninNamesOnIOThread::Internal {
 private:
  virtual ~TestInternal() {}

  // Disable thread checks for testing.
  virtual void CheckOnIOThread() const OVERRIDE {}
  virtual void CheckOnUIThread() const OVERRIDE {}
  virtual void PostTaskToIOThread(int type, const string16& email) OVERRIDE;
};

void TestInternal::PostTaskToIOThread(int type, const string16& email) {
  // For tests, don't bother dispatching to IO thread.
  UpdateOnIOThread(type, email);
}

class TestSigninNamesOnIOThread : public SigninNamesOnIOThread {
  Internal* CreateInternal() OVERRIDE { return new TestInternal(); }
};

class SigninNamesOnIOThreadTest : public testing::Test {
 public:
  SigninNamesOnIOThreadTest();
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void SimulateSignin(const string16& email);
  void SimulateSignout(const string16& email);
  void AddNewProfile(const string16& name, const string16& email);

  TestingProfileManager testing_profile_manager_;
  TestSigninNamesOnIOThread signin_names_;
};

SigninNamesOnIOThreadTest::SigninNamesOnIOThreadTest()
    : testing_profile_manager_(
          static_cast<TestingBrowserProcess*>(g_browser_process)) {
}

void SigninNamesOnIOThreadTest::SetUp() {
  ASSERT_TRUE(testing_profile_manager_.SetUp());
  signin_names_.Initialize();
}

void SigninNamesOnIOThreadTest::TearDown() {
  signin_names_.ReleaseResources();
}

void SigninNamesOnIOThreadTest::SimulateSignin(const string16& email) {
  GoogleServiceSigninSuccessDetails details(UTF16ToUTF8(email), "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(NULL),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
}

void SigninNamesOnIOThreadTest::SimulateSignout(const string16& email) {
  GoogleServiceSignoutDetails details(UTF16ToUTF8(email));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
      content::Source<Profile>(NULL),
      content::Details<const GoogleServiceSignoutDetails>(&details));
}

void SigninNamesOnIOThreadTest::AddNewProfile(const string16& name,
                                              const string16& email) {
  ProfileInfoCache* cache = testing_profile_manager_.profile_info_cache();
  const FilePath& user_data_dir = cache->GetUserDataDir();
#if defined(OS_POSIX)
  const FilePath profile_dir = user_data_dir.Append(UTF16ToUTF8(name));
#else
  const FilePath profile_dir = user_data_dir.Append(name);
#endif
  cache->AddProfileToCache(profile_dir, name, email, 0);
}

}  // namespace

TEST_F(SigninNamesOnIOThreadTest, Basic) {
  ASSERT_EQ(0u, signin_names_.GetEmails().size());
}

TEST_F(SigninNamesOnIOThreadTest, Signin) {
  const string16 email = UTF8ToUTF16("foo@gmail.com");
  SimulateSignin(email);

  const SigninNamesOnIOThread::EmailSet& emails = signin_names_.GetEmails();
  ASSERT_EQ(1u, emails.size());
  ASSERT_EQ(1u, emails.count(email));
}

TEST_F(SigninNamesOnIOThreadTest, Signout) {
  const string16 email = UTF8ToUTF16("foo@gmail.com");
  SimulateSignin(email);
  SimulateSignout(email);

  const SigninNamesOnIOThread::EmailSet& emails = signin_names_.GetEmails();
  ASSERT_EQ(0u, emails.size());
}

TEST_F(SigninNamesOnIOThreadTest, HandleUnknownSignout) {
  const string16 email = UTF8ToUTF16("foo@gmail.com");
  SimulateSignin(email);
  SimulateSignout(UTF8ToUTF16("bar@gmail.com"));

  const SigninNamesOnIOThread::EmailSet& emails = signin_names_.GetEmails();
  ASSERT_EQ(1u, emails.size());
  ASSERT_EQ(1u, emails.count(email));
}

TEST_F(SigninNamesOnIOThreadTest, StartWithConnectedProfiles) {
  // Setup a couple of connected profiles, and one unconnected.
  const string16 email1 = UTF8ToUTF16("foo@gmail.com");
  const string16 email2 = UTF8ToUTF16("bar@gmail.com");
  AddNewProfile(UTF8ToUTF16("foo"), email1);
  AddNewProfile(UTF8ToUTF16("bar"), email2);
  AddNewProfile(UTF8ToUTF16("toto"), string16());

  TestSigninNamesOnIOThread signin_names;
  signin_names.Initialize();

  const SigninNamesOnIOThread::EmailSet& emails = signin_names.GetEmails();
  ASSERT_EQ(2u, emails.size());
  ASSERT_EQ(1u, emails.count(email1));
  ASSERT_EQ(1u, emails.count(email2));

  signin_names.ReleaseResources();
}
