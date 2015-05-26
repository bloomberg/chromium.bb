// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/active_state_manager_impl.h"

#include "base/memory/scoped_ptr.h"
#include "ios/web/public/active_state_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class ActiveStateManagerImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_.reset(new TestBrowserState());
  }
  void TearDown() override {
    // The BrowserState needs to be destroyed first so that it outlives the
    // WebThreadBundle.
    if (browser_state_) {
      BrowserState::GetActiveStateManager(browser_state_.get())
          ->SetActive(false);
      browser_state_.reset();
    }
    PlatformTest::TearDown();
  }

  // The BrowserState used for testing purposes.
  scoped_ptr<BrowserState> browser_state_;

 private:
  // Used to create TestWebThreads.
  TestWebThreadBundle thread_bundle_;
};

// An ActiveStateManagerImpl::Observer used for testing purposes.
class ActiveStateManagerImplObserver : public ActiveStateManagerImpl::Observer {
 public:
  ActiveStateManagerImplObserver() {}
  virtual ~ActiveStateManagerImplObserver() {}

  // ActiveStateManagerImpl::Observer implementation.
  MOCK_METHOD0(OnActive, void());
  MOCK_METHOD0(OnInactive, void());
  MOCK_METHOD0(WillBeDestroyed, void());
};

}  // namespace

// Tests that an ActiveStateManagerImpl is succesfully created with a
// BrowserState and that it can be made active/inactive.
TEST_F(ActiveStateManagerImplTest, ActiveState) {
  ActiveStateManager* active_state_manager =
      BrowserState::GetActiveStateManager(browser_state_.get());
  ASSERT_TRUE(active_state_manager);

  EXPECT_FALSE(active_state_manager->IsActive());

  active_state_manager->SetActive(true);
  EXPECT_TRUE(active_state_manager->IsActive());

  // Make sure it is ok to SetActive(true) on an already active
  // ActiveStateManager.
  active_state_manager->SetActive(true);
  EXPECT_TRUE(active_state_manager->IsActive());

  active_state_manager->SetActive(false);
  EXPECT_FALSE(active_state_manager->IsActive());
}

// Tests that ActiveStateManagerImpl::Observer are notified correctly.
TEST_F(ActiveStateManagerImplTest, ObserverMethod) {
  scoped_ptr<ActiveStateManagerImplObserver> observer(
      new ActiveStateManagerImplObserver());
  ActiveStateManagerImpl* active_state_manager =
      static_cast<ActiveStateManagerImpl*>(
          BrowserState::GetActiveStateManager(browser_state_.get()));

  active_state_manager->AddObserver(observer.get());

  EXPECT_CALL(*observer, OnActive()).Times(1);
  EXPECT_CALL(*observer, OnInactive()).Times(1);
  EXPECT_CALL(*observer, WillBeDestroyed()).Times(1);

  active_state_manager->SetActive(true);
  active_state_manager->SetActive(false);
  // There is no need to explicitly remove the observer since this call takes
  // care of it.
  browser_state_.reset();
}

}  // namespace web
