// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/sync_notifier_factory.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/notifier/invalidation_version_tracker.h"
#include "chrome/browser/sync/notifier/mock_sync_notifier_observer.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/common/chrome_switches.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {
namespace {

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrictMock;

class SyncNotifierFactoryTest : public testing::Test {
 protected:
  SyncNotifierFactoryTest()
      : command_line_(CommandLine::NO_PROGRAM) {}
  virtual ~SyncNotifierFactoryTest() {}

  virtual void SetUp() OVERRIDE {
    factory_.reset(new SyncNotifierFactory(
        "fake_client_info",
        new TestURLRequestContextGetter(message_loop_.message_loop_proxy()),
        base::WeakPtr<sync_notifier::InvalidationVersionTracker>(),
        command_line_));
    message_loop_.RunAllPending();
  }

  virtual void TearDown() OVERRIDE {
    Mock::VerifyAndClearExpectations(&mock_observer_);
    message_loop_.RunAllPending();
    command_line_ = CommandLine(CommandLine::NO_PROGRAM);
  }

  MessageLoop message_loop_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  scoped_ptr<SyncNotifierFactory> factory_;
  CommandLine command_line_;
};

// Test basic creation of a NonBlockingInvalidationNotifier.
TEST_F(SyncNotifierFactoryTest, Basic) {
  scoped_ptr<SyncNotifier> notifier(factory_->CreateSyncNotifier());
  ASSERT_TRUE(notifier.get());
  notifier->AddObserver(&mock_observer_);
  notifier->RemoveObserver(&mock_observer_);
}

// Test basic creation of a P2PNotifier.
TEST_F(SyncNotifierFactoryTest, Basic_P2P) {
  command_line_.AppendSwitchASCII(switches::kSyncNotificationMethod, "p2p");
  scoped_ptr<SyncNotifier> notifier(factory_->CreateSyncNotifier());
  ASSERT_TRUE(notifier.get());
  notifier->AddObserver(&mock_observer_);
  notifier->RemoveObserver(&mock_observer_);
}

}  // namespace
}  // namespace sync_notifier
