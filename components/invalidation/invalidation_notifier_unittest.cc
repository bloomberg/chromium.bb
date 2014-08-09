// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_notifier.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "components/invalidation/fake_invalidation_handler.h"
#include "components/invalidation/fake_invalidation_state_tracker.h"
#include "components/invalidation/invalidation_state_tracker.h"
#include "components/invalidation/invalidator_test_template.h"
#include "components/invalidation/push_client_channel.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class InvalidationNotifierTestDelegate {
 public:
  InvalidationNotifierTestDelegate() {}

  ~InvalidationNotifierTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& invalidator_client_id,
      const std::string& initial_state,
      const base::WeakPtr<InvalidationStateTracker>&
          invalidation_state_tracker) {
    DCHECK(!invalidator_.get());
    scoped_ptr<notifier::PushClient> push_client(
        new notifier::FakePushClient());
    scoped_ptr<SyncNetworkChannel> network_channel(
        new PushClientChannel(push_client.Pass()));
    invalidator_.reset(
        new InvalidationNotifier(network_channel.Pass(),
                                 invalidator_client_id,
                                 UnackedInvalidationsMap(),
                                 initial_state,
                                 invalidation_state_tracker,
                                 base::MessageLoopProxy::current(),
                                 "fake_client_info"));
  }

  Invalidator* GetInvalidator() {
    return invalidator_.get();
  }

  void DestroyInvalidator() {
    // Stopping the invalidation notifier stops its scheduler, which deletes
    // any pending tasks without running them.  Some tasks "run and delete"
    // another task, so they must be run in order to avoid leaking the inner
    // task.  Stopping does not schedule any tasks, so it's both necessary and
    // sufficient to drain the task queue before stopping the notifier.
    message_loop_.RunUntilIdle();
    invalidator_.reset();
  }

  void WaitForInvalidator() {
    message_loop_.RunUntilIdle();
  }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->OnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) {
    invalidator_->OnInvalidate(invalidation_map);
  }

 private:
  base::MessageLoop message_loop_;
  scoped_ptr<InvalidationNotifier> invalidator_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    InvalidationNotifierTest, InvalidatorTest,
    InvalidationNotifierTestDelegate);

}  // namespace

}  // namespace syncer
