// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// ChromeOS has its own network delay logic.
#if !defined(OS_CHROMEOS)

namespace {

class MockNetworkChangeNotifierNeverOffline :
    public net::NetworkChangeNotifier {
 public:
  net::NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() const
      override {
    return NetworkChangeNotifier::CONNECTION_3G;
  }
};

class MockNetworkChangeNotifierOfflineUntilChange :
    public net::NetworkChangeNotifier {
 public:
  MockNetworkChangeNotifierOfflineUntilChange() : online_(false) {}
  net::NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() const
      override {
    return online_ ? net::NetworkChangeNotifier::CONNECTION_3G
                   : net::NetworkChangeNotifier::CONNECTION_NONE;
  }

  void GoOnline() {
    online_ = true;
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_3G);
  }

 private:
  bool online_;
};

class CallbackTester {
 public:
  CallbackTester() : called_(0) {}

  void Increment();
  bool WasCalledExactlyOnce();

 private:
  int called_;
};

void CallbackTester::Increment() {
  called_++;
}

bool CallbackTester::WasCalledExactlyOnce() {
  return called_ == 1;
}

}  // namespace

class ChromeSigninClientTest : public testing::Test {
 public:
  ChromeSigninClientTest() {}
  void SetUp() override;

  Profile* profile() { return profile_.get(); }
  SigninClient* signin_client() { return signin_client_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<Profile> profile_;
  SigninClient* signin_client_;
};

void ChromeSigninClientTest::SetUp() {
  // Create a signed-in profile.
  TestingProfile::Builder builder;
  profile_ = builder.Build();

  signin_client_ = ChromeSigninClientFactory::GetForProfile(profile());
}

TEST_F(ChromeSigninClientTest, DelayNetworkCallRunsImmediatelyWithNetwork) {
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifierNeverOffline);
  CallbackTester tester;
  signin_client()->DelayNetworkCall(base::Bind(&CallbackTester::Increment,
                                               base::Unretained(&tester)));
  ASSERT_TRUE(tester.WasCalledExactlyOnce());
}

TEST_F(ChromeSigninClientTest, DelayNetworkCallRunsAfterNetworkFound) {
  std::unique_ptr<MockNetworkChangeNotifierOfflineUntilChange> mock(
      new MockNetworkChangeNotifierOfflineUntilChange());
  // Install a SigninClient after the NetworkChangeNotifier's created.
  SetUp();

  CallbackTester tester;
  signin_client()->DelayNetworkCall(base::Bind(&CallbackTester::Increment,
                                               base::Unretained(&tester)));
  ASSERT_FALSE(tester.WasCalledExactlyOnce());
  mock->GoOnline();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(tester.WasCalledExactlyOnce());
}

#if !defined(OS_ANDROID)

class MockChromeSigninClient : public ChromeSigninClient {
 public:
  MockChromeSigninClient(Profile* profile, SigninErrorController* controller)
      : ChromeSigninClient(profile, controller) {}

  MOCK_METHOD1(ShowUserManager, void(const base::FilePath&));
  MOCK_METHOD1(LockForceSigninProfile, void(const base::FilePath&));
};

class MockSigninManager : public SigninManager {
 public:
  explicit MockSigninManager(SigninClient* client)
      : SigninManager(client, nullptr, &fake_service_, nullptr) {}

  MOCK_METHOD2(DoSignOut,
               void(signin_metrics::ProfileSignout,
                    signin_metrics::SignoutDelete));

  AccountTrackerService fake_service_;
};

class ChromeSigninClientSignoutTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    prefs_.reset(new TestingPrefServiceSimple());
    chrome::RegisterLocalState(prefs_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(prefs_.get());
    prefs_->SetBoolean(prefs::kForceBrowserSignin, true);

    CreateClient(browser()->profile());
    manager_.reset(new MockSigninManager(client_.get()));
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void CreateClient(Profile* profile) {
    SigninErrorController* controller = new SigninErrorController();
    client_.reset(new MockChromeSigninClient(profile, controller));
    fake_controller_.reset(controller);
  }

  std::unique_ptr<SigninErrorController> fake_controller_;
  std::unique_ptr<MockChromeSigninClient> client_;
  std::unique_ptr<MockSigninManager> manager_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(ChromeSigninClientSignoutTest, SignOut) {
  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::SIGNOUT_TEST;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*manager_, DoSignOut(source_metric, delete_metric)).Times(1);

  manager_->SignOut(source_metric, delete_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutWithoutManager) {
  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::SIGNOUT_TEST;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  MockSigninManager other_manager(client_.get());
  other_manager.CopyCredentialsFrom(*manager_.get());

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*manager_, DoSignOut(source_metric, delete_metric)).Times(1);
  manager_->SignOut(source_metric, delete_metric);

  ::testing::Mock::VerifyAndClearExpectations(manager_.get());

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*manager_, DoSignOut(source_metric, delete_metric)).Times(1);
  manager_->SignOut(source_metric, delete_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutWithoutForceSignin) {
  prefs_->SetBoolean(prefs::kForceBrowserSignin, false);
  CreateClient(browser()->profile());
  manager_.reset(new MockSigninManager(client_.get()));

  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::SIGNOUT_TEST;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*manager_, DoSignOut(source_metric, delete_metric)).Times(1);
  manager_->SignOut(source_metric, delete_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutGuestSession) {
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  manager_.reset(new MockSigninManager(client_.get()));

  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::SIGNOUT_TEST;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*manager_, DoSignOut(source_metric, delete_metric)).Times(1);
  manager_->SignOut(source_metric, delete_metric);
}

#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_CHROMEOS)
