// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestLoginUI : public LoginUIService::LoginUI {
 public:
  TestLoginUI() { }
  virtual ~TestLoginUI() { }
  virtual void FocusUI() OVERRIDE { }
  virtual void CloseUI() OVERRIDE { }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLoginUI);
};

class TestObserver : public LoginUIService::Observer {
 public:
  TestObserver() : ui_shown_count_(0),
                   ui_closed_count_(0) {
  }

  int ui_shown_count() const { return ui_shown_count_; }
  int ui_closed_count() const { return ui_closed_count_; }

 private:
  virtual void OnLoginUIShown(LoginUIService::LoginUI* ui) OVERRIDE {
    ++ui_shown_count_;
  }

  virtual void OnLoginUIClosed(LoginUIService::LoginUI* ui) OVERRIDE {
    ++ui_closed_count_;
  }

  int ui_shown_count_;
  int ui_closed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class LoginUIServiceTest : public testing::Test {
 public:
  LoginUIServiceTest() : service_(NULL) { }
  virtual ~LoginUIServiceTest() { }

 protected:
  LoginUIService service_;
  TestObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(LoginUIServiceTest);
};

// Test that the observer is called back when login UI is shown
// or closed.
TEST_F(LoginUIServiceTest, LoginUIServiceObserver) {
  service_.AddObserver(&observer_);
  EXPECT_EQ(NULL, service_.current_login_ui());
  TestLoginUI ui;
  service_.SetLoginUI(&ui);
  EXPECT_EQ(1, observer_.ui_shown_count());

  // If a different UI is closed than the one that was shown, no
  // notification should be fired.
  TestLoginUI other_ui;
  service_.LoginUIClosed(&other_ui);
  EXPECT_EQ(1, observer_.ui_shown_count());

  // If the right UI is closed, notification should be fired.
  service_.LoginUIClosed(&ui);
  EXPECT_EQ(1, observer_.ui_closed_count());
  service_.RemoveObserver(&observer_);
}
