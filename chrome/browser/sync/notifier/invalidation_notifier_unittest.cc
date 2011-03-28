// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/invalidation_notifier.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/mock_sync_notifier_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
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
  InvalidationNotifierTest()
      : host_resolver_(
          net::CreateSystemHostResolver(
              net::HostResolver::kDefaultParallelism, NULL, NULL)),
        invalidation_notifier_(notifier::NotifierOptions(),
                               host_resolver_.get(),
                               &cert_verifier_,
                               "fake_client_info") {}

 protected:
  virtual void SetUp() {
    invalidation_notifier_.AddObserver(&mock_observer_);
  }

  virtual void TearDown() {
    invalidation_notifier_.RemoveObserver(&mock_observer_);
  }

  MessageLoop message_loop_;
  scoped_ptr<net::HostResolver> host_resolver_;
  net::CertVerifier cert_verifier_;
  InvalidationNotifier invalidation_notifier_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  notifier::FakeBaseTask fake_base_task_;
};

TEST_F(InvalidationNotifierTest, Basic) {
  InSequence dummy;

  syncable::ModelTypeSet types;
  types.insert(syncable::BOOKMARKS);
  types.insert(syncable::AUTOFILL);

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_, StoreState("new_fake_state"));
  {
    syncable::ModelTypePayloadMap type_payloads;
    type_payloads[syncable::PREFERENCES] = "payload";
    EXPECT_CALL(mock_observer_, OnIncomingNotification(type_payloads));
  }
  {
    syncable::ModelTypePayloadMap type_payloads;
    type_payloads[syncable::BOOKMARKS] = "";
    type_payloads[syncable::AUTOFILL] = "";
    EXPECT_CALL(mock_observer_, OnIncomingNotification(type_payloads));
  }
  EXPECT_CALL(mock_observer_, OnNotificationStateChange(false));

  invalidation_notifier_.SetState("fake_state");
  invalidation_notifier_.UpdateCredentials("foo@bar.com", "fake_token");
  invalidation_notifier_.UpdateEnabledTypes(types);

  invalidation_notifier_.OnConnect(fake_base_task_.AsWeakPtr());

  invalidation_notifier_.WriteState("new_fake_state");

  // Even though preferences isn't in the set of enabled types, we
  // should still propagate the notification.
  invalidation_notifier_.OnInvalidate(syncable::PREFERENCES, "payload");
  invalidation_notifier_.OnInvalidateAll();

  invalidation_notifier_.OnDisconnect();
}

TEST_F(InvalidationNotifierTest, UpdateEnabledTypes) {
  InSequence dummy;

  syncable::ModelTypeSet types;
  types.insert(syncable::BOOKMARKS);
  types.insert(syncable::AUTOFILL);

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  {
    syncable::ModelTypePayloadMap type_payloads;
    type_payloads[syncable::BOOKMARKS] = "";
    type_payloads[syncable::AUTOFILL] = "";
    EXPECT_CALL(mock_observer_, OnIncomingNotification(type_payloads));
  }

  invalidation_notifier_.OnConnect(fake_base_task_.AsWeakPtr());
  invalidation_notifier_.UpdateEnabledTypes(types);
  invalidation_notifier_.OnInvalidateAll();
}

}  // namespace

}  // namespace sync_notifier
