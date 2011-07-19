// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/invalidation_notifier.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/mock_sync_notifier_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/test/test_url_request_context_getter.h"
#include "content/browser/browser_thread.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class InvalidationNotifierTest : public testing::Test {
 public:
  InvalidationNotifierTest() : io_thread_(BrowserThread::IO, &message_loop_) {}

 protected:
  virtual void SetUp() {
    request_context_getter_ = new TestURLRequestContextGetter;
    notifier::NotifierOptions notifier_options;
    notifier_options.request_context_getter = request_context_getter_;
    invalidation_notifier_.reset(new InvalidationNotifier(notifier_options,
                                                          "fake_client_info"));
    invalidation_notifier_->AddObserver(&mock_observer_);
  }

  virtual void TearDown() {
    invalidation_notifier_->RemoveObserver(&mock_observer_);
    invalidation_notifier_.reset();
    request_context_getter_ = NULL;
  }

  MessageLoop message_loop_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<InvalidationNotifier> invalidation_notifier_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  notifier::FakeBaseTask fake_base_task_;
  // Since this test calls HostResolver code, we need an IO thread.
  BrowserThread io_thread_;
};

TEST_F(InvalidationNotifierTest, Basic) {
  InSequence dummy;

  syncable::ModelTypePayloadMap type_payloads;
  type_payloads[syncable::PREFERENCES] = "payload";
  type_payloads[syncable::BOOKMARKS] = "";
  type_payloads[syncable::AUTOFILL] = "";

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_, StoreState("new_fake_state"));
  EXPECT_CALL(mock_observer_, OnIncomingNotification(type_payloads));
  EXPECT_CALL(mock_observer_, OnNotificationStateChange(false));

  invalidation_notifier_->SetState("fake_state");
  invalidation_notifier_->UpdateCredentials("foo@bar.com", "fake_token");

  invalidation_notifier_->OnConnect(fake_base_task_.AsWeakPtr());
  invalidation_notifier_->OnSessionStatusChanged(true);

  invalidation_notifier_->WriteState("new_fake_state");

  invalidation_notifier_->OnInvalidate(type_payloads);

  // Shouldn't trigger notification state change.
  invalidation_notifier_->OnDisconnect();
  invalidation_notifier_->OnConnect(fake_base_task_.AsWeakPtr());

  invalidation_notifier_->OnSessionStatusChanged(false);
}

}  // namespace

}  // namespace sync_notifier
