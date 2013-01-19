// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::StrictMock;

namespace {

class FakeManagedMode : public ManagedMode {
 public:
  FakeManagedMode() : in_managed_mode_(false), should_cancel_enter_(false) {
  }

  void set_should_cancel_enter(bool should_cancel_enter) {
    should_cancel_enter_ = should_cancel_enter;
  }

  void EnterManagedModeForTesting(Profile* profile,
                                  const base::Callback<void(bool)>& callback) {
    EnterManagedModeImpl(profile, callback);
  }

  // ManagedMode overrides:
  virtual bool IsInManagedModeImpl() const OVERRIDE {
    return in_managed_mode_;
  }

  virtual void SetInManagedMode(Profile* newly_managed_profile) OVERRIDE {
    if (newly_managed_profile) {
      ASSERT_TRUE(!managed_profile_ ||
                  managed_profile_ == newly_managed_profile);
    }
    managed_profile_ = newly_managed_profile;
    in_managed_mode_ = (newly_managed_profile != NULL);
  }

  virtual bool PlatformConfirmEnter() OVERRIDE {
    return !should_cancel_enter_;
  }

  virtual bool PlatformConfirmLeave() OVERRIDE {
    return true;
  }

 private:
  bool in_managed_mode_;
  bool should_cancel_enter_;
};

class MockBrowserWindow : public TestBrowserWindow {
 public:
  MOCK_METHOD0(Close, void());
};

class BrowserFixture {
 public:
  BrowserFixture(FakeManagedMode* managed_mode,
                 TestingProfile* profile) {
    Browser::CreateParams params(profile);
    params.window = &window_;
    browser_.reset(new Browser(params));
  }

  ~BrowserFixture() {
  }

  MockBrowserWindow* window() {
    return &window_;
  }

 private:
  scoped_ptr<Browser> browser_;
  StrictMock<MockBrowserWindow> window_;
};

class MockCallback : public base::RefCountedThreadSafe<MockCallback> {
 public:
  explicit MockCallback(FakeManagedMode* managed_mode)
      : managed_mode_(managed_mode) {
  }

  void CheckManagedMode(bool success) {
    EXPECT_EQ(success, managed_mode_->IsInManagedModeImpl());
    DidEnterManagedMode(success);
  }

  MOCK_METHOD1(DidEnterManagedMode, void(bool));

 protected:
  virtual ~MockCallback() {}

 private:
  friend class base::RefCountedThreadSafe<MockCallback>;

  FakeManagedMode* managed_mode_;

  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

}  // namespace

class ManagedModeTest : public ::testing::Test {
 public:
  ManagedModeTest() : message_loop_(MessageLoop::TYPE_UI),
                      ui_thread_(content::BrowserThread::UI, &message_loop_),
                      io_thread_(content::BrowserThread::IO, &message_loop_) {
  }

  scoped_refptr<MockCallback> CreateCallback() {
    return new StrictMock<MockCallback>(&managed_mode_);
  }

  base::Callback<void(bool)> CreateExpectedCallback(bool success) {
    scoped_refptr<MockCallback> callback = CreateCallback();
    EXPECT_CALL(*callback, DidEnterManagedMode(success));
    return base::Bind(&MockCallback::CheckManagedMode, callback);
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  TestingProfile managed_mode_profile_;
  TestingProfile other_profile_;
  FakeManagedMode managed_mode_;
};

TEST_F(ManagedModeTest, SingleBrowser) {
  BrowserFixture managed_mode_browser(&managed_mode_, &managed_mode_profile_);

  // If there are no browsers belonging to a different profile, entering should
  // immediately succeed.
  managed_mode_.EnterManagedModeForTesting(&managed_mode_profile_,
                                           CreateExpectedCallback(true));
}

TEST_F(ManagedModeTest, AlreadyInManagedMode) {
  BrowserFixture managed_mode_browser(&managed_mode_, &managed_mode_profile_);
  BrowserFixture other_browser(&managed_mode_, &other_profile_);

  // If we're already in managed mode in this profile, entering should
  // immediately succeed.
  managed_mode_.SetInManagedMode(&managed_mode_profile_);
  managed_mode_.EnterManagedModeForTesting(&managed_mode_profile_,
                                           CreateExpectedCallback(true));
}

TEST_F(ManagedModeTest, QueueRequests) {
  BrowserFixture managed_mode_browser(&managed_mode_, &managed_mode_profile_);

  // Keep this before the other browser is constructed, so we verify its
  // expectations after the browser is destroyed.
  scoped_refptr<MockCallback> callback = CreateCallback();
  BrowserFixture other_browser(&managed_mode_, &other_profile_);

  EXPECT_CALL(*other_browser.window(), Close());
  managed_mode_.EnterManagedModeForTesting(
      &managed_mode_profile_,
      base::Bind(&MockCallback::CheckManagedMode, callback));
  managed_mode_.EnterManagedModeForTesting(
      &managed_mode_profile_,
      base::Bind(&MockCallback::CheckManagedMode, callback));
  // The callbacks should run as soon as |other_browser| is closed.
  EXPECT_CALL(*callback, DidEnterManagedMode(true)).Times(2);
}

TEST_F(ManagedModeTest, OpenNewBrowser) {
  BrowserFixture managed_mode_browser(&managed_mode_, &managed_mode_profile_);
  BrowserFixture other_browser(&managed_mode_, &other_profile_);

  scoped_refptr<MockCallback> callback = CreateCallback();
  EXPECT_CALL(*other_browser.window(), Close());
  managed_mode_.EnterManagedModeForTesting(
      &managed_mode_profile_,
      base::Bind(&MockCallback::CheckManagedMode, callback));

  // Opening another browser with the managed profile should not cancel entering
  // managed mode.
  BrowserFixture other_managed_mode_browser(&managed_mode_,
                                            &managed_mode_profile_);

  // Opening another browser should cancel entering managed mode.
  EXPECT_CALL(*callback, DidEnterManagedMode(false));
  BrowserFixture yet_another_browser(&managed_mode_, &other_profile_);
}

TEST_F(ManagedModeTest, DifferentProfile) {
  BrowserFixture managed_mode_browser(&managed_mode_, &managed_mode_profile_);

  // Keep this before the other browser is constructed, so we verify its
  // expectations after the browser is destroyed.
  scoped_refptr<MockCallback> callback = CreateCallback();
  BrowserFixture other_browser(&managed_mode_, &other_profile_);

  EXPECT_CALL(*other_browser.window(), Close());
  managed_mode_.EnterManagedModeForTesting(
      &managed_mode_profile_,
      base::Bind(&MockCallback::CheckManagedMode, callback));

  // Trying to enter managed mode with a different profile should fail
  // immediately.
  managed_mode_.EnterManagedModeForTesting(&other_profile_,
                                           CreateExpectedCallback(false));

  // The first request should still succeed as soon as the other browser is
  // closed.
  EXPECT_CALL(*callback, DidEnterManagedMode(true));
}

TEST_F(ManagedModeTest, Cancelled) {
  BrowserFixture managed_mode_browser(&managed_mode_, &managed_mode_profile_);
  BrowserFixture other_browser(&managed_mode_, &other_profile_);

  // If the user cancelled entering managed mode, it should fail immediately.
  managed_mode_.set_should_cancel_enter(true);
  managed_mode_.EnterManagedModeForTesting(&managed_mode_profile_,
                                           CreateExpectedCallback(false));
}
