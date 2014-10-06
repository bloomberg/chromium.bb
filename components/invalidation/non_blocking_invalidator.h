// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of SyncNotifier that wraps InvalidationNotifier
// on its own thread.

#ifndef COMPONENTS_INVALIDATION_NON_BLOCKING_INVALIDATOR_H_
#define COMPONENTS_INVALIDATION_NON_BLOCKING_INVALIDATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/invalidation_export.h"
#include "components/invalidation/invalidation_state_tracker.h"
#include "components/invalidation/invalidator.h"
#include "components/invalidation/invalidator_registrar.h"
#include "components/invalidation/invalidator_state.h"
#include "components/invalidation/unacked_invalidation_set.h"
#include "jingle/notifier/base/notifier_options.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace syncer {
class SyncNetworkChannel;
class GCMNetworkChannelDelegate;

// Callback type for function that creates SyncNetworkChannel. This function
// gets passed into NonBlockingInvalidator constructor.
typedef base::Callback<scoped_ptr<SyncNetworkChannel>(void)>
    NetworkChannelCreator;

class INVALIDATION_EXPORT_PRIVATE NonBlockingInvalidator
    : public Invalidator,
      public InvalidationStateTracker {
 public:
  // |invalidation_state_tracker| must be initialized and must outlive |this|.
  NonBlockingInvalidator(
      NetworkChannelCreator network_channel_creator,
      const std::string& invalidator_client_id,
      const UnackedInvalidationsMap& saved_invalidations,
      const std::string& invalidation_bootstrap_data,
      InvalidationStateTracker* invalidation_state_tracker,
      const std::string& client_info,
      const scoped_refptr<net::URLRequestContextGetter>&
          request_context_getter);

  virtual ~NonBlockingInvalidator();

  // Invalidator implementation.
  virtual void RegisterHandler(InvalidationHandler* handler) override;
  virtual void UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids) override;
  virtual void UnregisterHandler(InvalidationHandler* handler) override;
  virtual InvalidatorState GetInvalidatorState() const override;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) override;
  virtual void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> callback) const
      override;

  // Static functions to construct callback that creates network channel for
  // SyncSystemResources. The goal is to pass network channel to invalidator at
  // the same time not exposing channel specific parameters to invalidator and
  // channel implementation to client of invalidator.
  static NetworkChannelCreator MakePushClientChannelCreator(
      const notifier::NotifierOptions& notifier_options);
  static NetworkChannelCreator MakeGCMNetworkChannelCreator(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_ptr<GCMNetworkChannelDelegate> delegate);

  // These methods are forwarded to the invalidation_state_tracker_.
  virtual void ClearAndSetNewClientId(const std::string& data) override;
  virtual std::string GetInvalidatorClientId() const override;
  virtual void SetBootstrapData(const std::string& data) override;
  virtual std::string GetBootstrapData() const override;
  virtual void SetSavedInvalidations(
      const UnackedInvalidationsMap& states) override;
  virtual UnackedInvalidationsMap GetSavedInvalidations() const override;
  virtual void Clear() override;

 private:
  void OnInvalidatorStateChange(InvalidatorState state);
  void OnIncomingInvalidation(const ObjectIdInvalidationMap& invalidation_map);
  std::string GetOwnerName() const;

  friend class NonBlockingInvalidatorTestDelegate;

  struct InitializeOptions;
  class Core;

  InvalidatorRegistrar registrar_;
  InvalidationStateTracker* invalidation_state_tracker_;

  // The real guts of NonBlockingInvalidator, which allows this class to live
  // completely on the parent thread.
  scoped_refptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::WeakPtrFactory<NonBlockingInvalidator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingInvalidator);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_NON_BLOCKING_INVALIDATOR_H_
