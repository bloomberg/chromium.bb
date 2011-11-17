// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/non_blocking_invalidation_notifier.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/invalidation_version_tracker.h"
#include "chrome/browser/sync/notifier/mock_sync_notifier_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/browser/sync/util/weak_handle.h"
#include "chrome/test/base/test_url_request_context_getter.h"
#include "content/test/test_browser_thread.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;
using content::BrowserThread;

class NonBlockingInvalidationNotifierTest : public testing::Test {
 public:
  NonBlockingInvalidationNotifierTest()
      : io_thread_(BrowserThread::IO) {}

 protected:
  virtual void SetUp() {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartIOThread();
    request_context_getter_ = new TestURLRequestContextGetter;
    notifier::NotifierOptions notifier_options;
    notifier_options.request_context_getter = request_context_getter_;
    invalidation_notifier_.reset(
        new NonBlockingInvalidationNotifier(
            notifier_options,
            InvalidationVersionMap(),
            browser_sync::MakeWeakHandle(
                base::WeakPtr<sync_notifier::InvalidationVersionTracker>()),
            "fake_client_info"));
    invalidation_notifier_->AddObserver(&mock_observer_);
  }

  virtual void TearDown() {
    invalidation_notifier_->RemoveObserver(&mock_observer_);
    invalidation_notifier_.reset();
    request_context_getter_ = NULL;
    io_thread_.Stop();
    ui_loop_.RunAllPending();
  }

  MessageLoop ui_loop_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<NonBlockingInvalidationNotifier> invalidation_notifier_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  notifier::FakeBaseTask fake_base_task_;
};

TEST_F(NonBlockingInvalidationNotifierTest, Basic) {
  syncable::ModelTypeSet types;
  types.insert(syncable::BOOKMARKS);
  types.insert(syncable::AUTOFILL);

  invalidation_notifier_->SetUniqueId("fake_id");
  invalidation_notifier_->SetState("fake_state");
  invalidation_notifier_->UpdateCredentials("foo@bar.com", "fake_token");
  invalidation_notifier_->UpdateEnabledTypes(types);
}

// TODO(akalin): Add synchronous operations for testing to
// NonBlockingInvalidationNotifierTest and use that to test it.

}  // namespace

}  // namespace sync_notifier
