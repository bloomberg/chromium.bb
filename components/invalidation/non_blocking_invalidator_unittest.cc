// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/non_blocking_invalidator.h"

#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "components/invalidation/fake_invalidation_handler.h"
#include "components/invalidation/invalidation_state_tracker.h"
#include "components/invalidation/invalidator_test_template.h"
#include "google/cacheinvalidation/types.pb.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class NonBlockingInvalidatorTestDelegate {
 public:
  NonBlockingInvalidatorTestDelegate() : io_thread_("IO thread") {}

  ~NonBlockingInvalidatorTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& invalidator_client_id,
      const std::string& initial_state,
      const base::WeakPtr<InvalidationStateTracker>&
          invalidation_state_tracker) {
    DCHECK(!invalidator_.get());
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
    request_context_getter_ =
        new net::TestURLRequestContextGetter(io_thread_.message_loop_proxy());
    notifier::NotifierOptions notifier_options;
    notifier_options.request_context_getter = request_context_getter_;
    NetworkChannelCreator network_channel_creator =
        NonBlockingInvalidator::MakePushClientChannelCreator(notifier_options);
    invalidator_.reset(
        new NonBlockingInvalidator(
            network_channel_creator,
            invalidator_client_id,
            UnackedInvalidationsMap(),
            initial_state,
            invalidation_state_tracker.get(),
            "fake_client_info",
            request_context_getter_));
  }

  Invalidator* GetInvalidator() {
    return invalidator_.get();
  }

  void DestroyInvalidator() {
    invalidator_.reset();
    request_context_getter_ = NULL;
    io_thread_.Stop();
    message_loop_.RunUntilIdle();
  }

  void WaitForInvalidator() {
    base::RunLoop run_loop;
    ASSERT_TRUE(
        io_thread_.message_loop_proxy()->PostTaskAndReply(
            FROM_HERE,
            base::Bind(&base::DoNothing),
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->OnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) {
    invalidator_->OnIncomingInvalidation(invalidation_map);
  }

 private:
  base::MessageLoop message_loop_;
  base::Thread io_thread_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<NonBlockingInvalidator> invalidator_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    NonBlockingInvalidatorTest, InvalidatorTest,
    NonBlockingInvalidatorTestDelegate);

}  // namespace syncer
