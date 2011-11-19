// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/non_blocking_invalidation_notifier.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/notifier/invalidation_notifier.h"

namespace sync_notifier {

class NonBlockingInvalidationNotifier::Core
    : public base::RefCountedThreadSafe<NonBlockingInvalidationNotifier::Core>,
      public SyncNotifierObserver {
 public:
  // Called on parent thread.  |delegate_observer| should be
  // initialized.
  explicit Core(
      const browser_sync::WeakHandle<SyncNotifierObserver>&
          delegate_observer);

  // Helpers called on I/O thread.
  void Initialize(
      const notifier::NotifierOptions& notifier_options,
      const InvalidationVersionMap& initial_max_invalidation_versions,
      const browser_sync::WeakHandle<InvalidationVersionTracker>&
          invalidation_version_tracker,
      const std::string& client_info);
  void Teardown();
  void SetUniqueId(const std::string& unique_id);
  void SetState(const std::string& state);
  void UpdateCredentials(const std::string& email, const std::string& token);
  void UpdateEnabledTypes(const syncable::ModelTypeSet& enabled_types);

  // SyncNotifierObserver implementation (all called on I/O thread).
  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads);
  virtual void OnNotificationStateChange(bool notifications_enabled);
  virtual void StoreState(const std::string& state);

 private:
  friend class
      base::RefCountedThreadSafe<NonBlockingInvalidationNotifier::Core>;
  // Called on parent or I/O thread.
  ~Core();

  // The variables below should be used only on the I/O thread.
  const browser_sync::WeakHandle<SyncNotifierObserver> delegate_observer_;
  scoped_ptr<InvalidationNotifier> invalidation_notifier_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingInvalidationNotifier::Core::Core(
    const browser_sync::WeakHandle<SyncNotifierObserver>&
        delegate_observer)
    : delegate_observer_(delegate_observer) {
  DCHECK(delegate_observer_.IsInitialized());
}

NonBlockingInvalidationNotifier::Core::~Core() {
}

void NonBlockingInvalidationNotifier::Core::Initialize(
    const notifier::NotifierOptions& notifier_options,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const browser_sync::WeakHandle<InvalidationVersionTracker>&
        invalidation_version_tracker,
    const std::string& client_info) {
  DCHECK(notifier_options.request_context_getter);
  DCHECK_EQ(notifier::NOTIFICATION_SERVER,
            notifier_options.notification_method);
  io_message_loop_proxy_ = notifier_options.request_context_getter->
      GetIOMessageLoopProxy();
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_.reset(
      new InvalidationNotifier(
          notifier_options,
          initial_max_invalidation_versions,
          invalidation_version_tracker,
          client_info));
  invalidation_notifier_->AddObserver(this);
}


void NonBlockingInvalidationNotifier::Core::Teardown() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_->RemoveObserver(this);
  invalidation_notifier_.reset();
  io_message_loop_proxy_ = NULL;
}

void NonBlockingInvalidationNotifier::Core::SetUniqueId(
    const std::string& unique_id) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_->SetUniqueId(unique_id);
}

void NonBlockingInvalidationNotifier::Core::SetState(
    const std::string& state) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_->SetState(state);
}

void NonBlockingInvalidationNotifier::Core::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateCredentials(email, token);
}

void NonBlockingInvalidationNotifier::Core::UpdateEnabledTypes(
    const syncable::ModelTypeSet& enabled_types) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateEnabledTypes(enabled_types);
}

void NonBlockingInvalidationNotifier::Core::OnIncomingNotification(
        const syncable::ModelTypePayloadMap& type_payloads) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  delegate_observer_.Call(FROM_HERE,
                          &SyncNotifierObserver::OnIncomingNotification,
                          type_payloads);
}

void NonBlockingInvalidationNotifier::Core::OnNotificationStateChange(
        bool notifications_enabled) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  delegate_observer_.Call(FROM_HERE,
                          &SyncNotifierObserver::OnNotificationStateChange,
                          notifications_enabled);
}

void NonBlockingInvalidationNotifier::Core::StoreState(
    const std::string& state) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  delegate_observer_.Call(FROM_HERE,
                          &SyncNotifierObserver::StoreState, state);
}

NonBlockingInvalidationNotifier::NonBlockingInvalidationNotifier(
    const notifier::NotifierOptions& notifier_options,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const browser_sync::WeakHandle<InvalidationVersionTracker>&
        invalidation_version_tracker,
    const std::string& client_info)
        : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
          core_(
              new Core(browser_sync::MakeWeakHandle(
                  weak_ptr_factory_.GetWeakPtr()))),
          parent_message_loop_proxy_(
              base::MessageLoopProxy::current()),
          io_message_loop_proxy_(notifier_options.request_context_getter->
              GetIOMessageLoopProxy()) {
  if (!io_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(
              &NonBlockingInvalidationNotifier::Core::Initialize,
              core_.get(),
              notifier_options,
              initial_max_invalidation_versions,
              invalidation_version_tracker,
              client_info))) {
    NOTREACHED();
  }
}

NonBlockingInvalidationNotifier::~NonBlockingInvalidationNotifier() {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  if (!io_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::Teardown,
                     core_.get()))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::AddObserver(
    SyncNotifierObserver* observer) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  observers_.AddObserver(observer);
}

void NonBlockingInvalidationNotifier::RemoveObserver(
    SyncNotifierObserver* observer) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  observers_.RemoveObserver(observer);
}

void NonBlockingInvalidationNotifier::SetUniqueId(
    const std::string& unique_id) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  if (!io_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::SetUniqueId,
                     core_.get(), unique_id))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::SetState(const std::string& state) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  if (!io_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::SetState,
                     core_.get(), state))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  if (!io_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::UpdateCredentials,
                     core_.get(), email, token))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::UpdateEnabledTypes(
    const syncable::ModelTypeSet& enabled_types) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  if (!io_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidationNotifier::Core::UpdateEnabledTypes,
                     core_.get(), enabled_types))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidationNotifier::SendNotification(
    const syncable::ModelTypeSet& changed_types) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  // InvalidationClient doesn't implement SendNotification(), so no
  // need to forward on the call.
}

void NonBlockingInvalidationNotifier::OnIncomingNotification(
        const syncable::ModelTypePayloadMap& type_payloads) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnIncomingNotification(type_payloads));
}

void NonBlockingInvalidationNotifier::OnNotificationStateChange(
        bool notifications_enabled) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnNotificationStateChange(notifications_enabled));
}

void NonBlockingInvalidationNotifier::StoreState(
    const std::string& state) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    StoreState(state));
}

}  // namespace sync_notifier
