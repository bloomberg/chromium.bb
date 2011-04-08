// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/non_blocking_invalidation_notifier.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/mock_sync_notifier_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/test/test_url_request_context_getter.h"
#include "content/browser/browser_thread.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class NonBlockingInvalidationNotifierTest : public testing::Test {
 public:
  NonBlockingInvalidationNotifierTest() {}

 protected:
  virtual void SetUp() {
    request_context_getter_ = new TestURLRequestContextGetter;
    notifier::NotifierOptions notifier_options;
    notifier_options.request_context_getter = request_context_getter_;
    invalidation_notifier_.reset(
        new NonBlockingInvalidationNotifier(notifier_options,
                                            "fake_client_info"));
    invalidation_notifier_->AddObserver(&mock_observer_);
  }

  virtual void TearDown() {
    invalidation_notifier_->RemoveObserver(&mock_observer_);
    invalidation_notifier_.reset();
    {
      // The request context getter gets deleted on the I/O thread. To prevent a
      // leak supply one here.
      BrowserThread io_thread(BrowserThread::IO, MessageLoop::current());
      request_context_getter_ = NULL;
    }
    MessageLoop::current()->RunAllPending();
  }

  MessageLoop message_loop_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<NonBlockingInvalidationNotifier> invalidation_notifier_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  notifier::FakeBaseTask fake_base_task_;
};

TEST_F(NonBlockingInvalidationNotifierTest, Basic) {
  syncable::ModelTypeSet types;
  types.insert(syncable::BOOKMARKS);
  types.insert(syncable::AUTOFILL);

  invalidation_notifier_->SetState("fake_state");
  invalidation_notifier_->UpdateCredentials("foo@bar.com", "fake_token");
  invalidation_notifier_->UpdateEnabledTypes(types);
}

// TODO(akalin): Add synchronous operations for testing to
// NonBlockingInvalidationNotifierTest and use that to test it.

}  // namespace

}  // namespace sync_notifier
