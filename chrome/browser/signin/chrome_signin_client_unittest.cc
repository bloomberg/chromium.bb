// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_change_notifier.h"
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
  std::unique_ptr<Profile> profile_;
  SigninClient* signin_client_;
  content::TestBrowserThreadBundle thread_bundle_;
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

#endif
