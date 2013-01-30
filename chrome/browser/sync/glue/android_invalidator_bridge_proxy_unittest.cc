// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/android_invalidator_bridge_proxy.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/android_invalidator_bridge.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
class InvalidationStateTracker;
}  // namespace syncer

namespace browser_sync {

namespace {

// All tests just verify that each call is passed through to the delegate, with
// the exception of RegisterHandler, UnregisterHandler, and
// UpdateRegisteredIds, which also verifies the call is forwarded to the
// bridge.
class AndroidInvalidatorBridgeProxyTest : public testing::Test {
 public:
  AndroidInvalidatorBridgeProxyTest()
    : ui_thread_(content::BrowserThread::UI, &ui_loop_),
      bridge_(&mock_profile_, ui_loop_.message_loop_proxy()),
      invalidator_(new AndroidInvalidatorBridgeProxy(&bridge_)) {
    // Pump |ui_loop_| to fully initialize |bridge_|.
    ui_loop_.RunUntilIdle();
  }

  virtual ~AndroidInvalidatorBridgeProxyTest() {
    DestroyInvalidator();
  }

  AndroidInvalidatorBridge* GetBridge() {
    return &bridge_;
  }

  AndroidInvalidatorBridgeProxy* GetInvalidator() {
    return invalidator_.get();
  }

 protected:
  void DestroyInvalidator() {
    invalidator_.reset();
    bridge_.StopForShutdown();
    ui_loop_.RunUntilIdle();
  }

  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  ::testing::NiceMock<ProfileMock> mock_profile_;
  AndroidInvalidatorBridge bridge_;
  scoped_ptr<AndroidInvalidatorBridgeProxy> invalidator_;
};

TEST_F(AndroidInvalidatorBridgeProxyTest, HandlerMethods) {
  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(1, "id1"));

  syncer::FakeInvalidationHandler handler;

  GetInvalidator()->RegisterHandler(&handler);
  EXPECT_TRUE(GetBridge()->IsHandlerRegisteredForTest(&handler));

  GetInvalidator()->UpdateRegisteredIds(&handler, ids);
  EXPECT_EQ(ids, GetBridge()->GetRegisteredIdsForTest(&handler));

  GetInvalidator()->UnregisterHandler(&handler);
  EXPECT_FALSE(GetBridge()->IsHandlerRegisteredForTest(&handler));
}

TEST_F(AndroidInvalidatorBridgeProxyTest, GetInvalidatorState) {
  // The AndroidInvalidatorBridge never enters an error state.
  EXPECT_EQ(syncer::INVALIDATIONS_ENABLED,
            GetInvalidator()->GetInvalidatorState());
  EXPECT_EQ(syncer::INVALIDATIONS_ENABLED,
            GetBridge()->GetInvalidatorState());
}

}  // namespace
}  // namespace browser_sync

