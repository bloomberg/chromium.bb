// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of Invalidator that wraps an invalidation
// client.  Handles the details of connecting to XMPP and hooking it
// up to the invalidation client.
//
// You probably don't want to use this directly; use
// NonBlockingInvalidator.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_NOTIFIER_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_NOTIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "components/invalidation/invalidation_export.h"
#include "components/invalidation/invalidation_state_tracker.h"
#include "components/invalidation/invalidator.h"
#include "components/invalidation/invalidator_registrar.h"
#include "components/invalidation/sync_invalidation_listener.h"

namespace notifier {
class PushClient;
}  // namespace notifier

namespace syncer {

// This class must live on the IO thread.
class INVALIDATION_EXPORT_PRIVATE InvalidationNotifier
    : public Invalidator,
      public SyncInvalidationListener::Delegate,
      public base::NonThreadSafe {
 public:
  // |invalidation_state_tracker| must be initialized.
  InvalidationNotifier(
      scoped_ptr<SyncNetworkChannel> network_channel,
      const std::string& invalidator_client_id,
      const UnackedInvalidationsMap& saved_invalidations,
      const std::string& invalidation_bootstrap_data,
      const base::WeakPtr<InvalidationStateTracker>& invalidation_state_tracker,
      scoped_refptr<base::SingleThreadTaskRunner>
          invalidation_state_tracker_task_runner,
      const std::string& client_info);

  virtual ~InvalidationNotifier();

  // Invalidator implementation.
  virtual void RegisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> callback) const
      OVERRIDE;

  // SyncInvalidationListener::Delegate implementation.
  virtual void OnInvalidate(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE;
  virtual void OnInvalidatorStateChange(InvalidatorState state) OVERRIDE;

 private:
  // We start off in the STOPPED state.  When we get our initial
  // credentials, we connect and move to the CONNECTING state.  When
  // we're connected we start the invalidation client and move to the
  // STARTED state.  We never go back to a previous state.
  enum State {
    STOPPED,
    CONNECTING,
    STARTED
  };
  State state_;

  InvalidatorRegistrar registrar_;

  // Passed to |invalidation_listener_|.
  const UnackedInvalidationsMap saved_invalidations_;

  // Passed to |invalidation_listener_|.
  const base::WeakPtr<InvalidationStateTracker> invalidation_state_tracker_;
  scoped_refptr<base::SequencedTaskRunner>
      invalidation_state_tracker_task_runner_;

  // Passed to |invalidation_listener_|.
  const std::string client_info_;

  // The client ID to pass to |invalidation_listener_|.
  const std::string invalidator_client_id_;

  // The initial bootstrap data to pass to |invalidation_listener_|.
  const std::string invalidation_bootstrap_data_;

  // The invalidation listener.
  SyncInvalidationListener invalidation_listener_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationNotifier);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_NOTIFIER_H_
