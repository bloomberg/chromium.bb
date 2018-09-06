// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_launcher.h"

#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace browser_switcher {

namespace {

const char kExampleDotCom[] = "http://example.com/";

class MockAlternativeBrowserDriver : public AlternativeBrowserDriver {
 public:
  MockAlternativeBrowserDriver() = default;
  ~MockAlternativeBrowserDriver() override = default;

  MOCK_METHOD1(SetBrowserPath, void(base::StringPiece));
  MOCK_METHOD1(SetBrowserParameters, void(base::StringPiece));
  MOCK_METHOD1(TryLaunch, bool(const GURL&));
};

}  // namespace

class AlternativeBrowserLauncherTest : public testing::Test {
 public:
  void SetUp() override {
    prefs_.registry()->RegisterStringPref(prefs::kAlternativeBrowserPath, "");
    prefs_.registry()->RegisterStringPref(prefs::kAlternativeBrowserParameters,
                                          "");
    driver_ = new MockAlternativeBrowserDriver();
    EXPECT_CALL(*driver_, SetBrowserPath(_));
    EXPECT_CALL(*driver_, SetBrowserParameters(_));
    launcher_ = std::make_unique<AlternativeBrowserLauncher>(
        &prefs_, std::unique_ptr<AlternativeBrowserDriver>(driver_));
  }

  PrefService* prefs() { return &prefs_; }
  AlternativeBrowserLauncher* launcher() { return launcher_.get(); }
  MockAlternativeBrowserDriver& driver() { return *driver_; }

 private:
  TestingPrefServiceSimple prefs_;

  std::unique_ptr<AlternativeBrowserLauncher> launcher_;
  MockAlternativeBrowserDriver* driver_;
};

TEST_F(AlternativeBrowserLauncherTest, LaunchSucceeds) {
  EXPECT_CALL(driver(), TryLaunch(_)).WillOnce(Invoke([](const GURL& url) {
    EXPECT_EQ(kExampleDotCom, url.spec());
    return true;
  }));
  EXPECT_TRUE(launcher()->Launch(GURL(kExampleDotCom)));
}

TEST_F(AlternativeBrowserLauncherTest, LaunchFails) {
  EXPECT_CALL(driver(), TryLaunch(_)).WillOnce(Return(false));
  EXPECT_FALSE(launcher()->Launch(GURL(kExampleDotCom)));
}

TEST_F(AlternativeBrowserLauncherTest, LaunchPicksUpPrefChanges) {
  EXPECT_CALL(driver(), SetBrowserPath(_))
      .WillOnce(
          Invoke([](base::StringPiece path) { EXPECT_EQ("bogus.exe", path); }));
  prefs()->SetString(prefs::kAlternativeBrowserPath, "bogus.exe");
  EXPECT_CALL(driver(), SetBrowserParameters(_))
      .WillOnce(Invoke([](base::StringPiece parameters) {
        EXPECT_EQ("--single-process", parameters);
      }));
  prefs()->SetString(prefs::kAlternativeBrowserParameters, "--single-process");
}

}  // namespace browser_switcher
