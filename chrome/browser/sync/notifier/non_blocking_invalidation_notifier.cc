// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/non_blocking_invalidation_notifier.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/notifier/invalidation_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"

namespace sync_notifier {

class NonBlockingInvalidationNotifier::Core
    : public base::RefCountedThreadSafe<NonBlockingInvalidationNotifier::Core>,
      public SyncNotifierObserver {
 public:
  // Called on parent thread.
  Core();

  // Called on parent thread.
  void AddObserver(SyncNotifierObserver* observer);
  void RemoveObserver(SyncNotifierObserver* observer);

  // Helpers called on I/O thread.
  void Initialize(const notifier::NotifierOptions& notifier_options,
                  const std::string& client_info);
  void Teardown();
  void SetState(const std::string& state);
  void UpdateCredentials(const std::string& email, const std::string& token);
  void UpdateEnabledTypes(const syncable::ModelTypeSet& types);
  void SendNotification();

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

  scoped_ptr<InvalidationNotifier> invalidation_notifier_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  scoped_refptr<ObserverListThreadSafe<SyncNotifierObserver> > observers_;
  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingInvalidationNotifier::Core::Core()
    : observers_(new ObserverListThreadSafe<SyncNotifierObserver>()) {
}

NonBlockingInvalidationNotifier::Core::~Core() {
}

void NonBlockingInvalidationNotifier::Core::Initialize(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info) {
  DCHECK(notifier_options.request_context_getter);
  DCHECK_EQ(notifier::NOTIFICATION_SERVER,
            notifier_options.notification_method);
  io_message_loop_proxy_ = notifier_options.request_context_getter->
      GetIOMessageLoopProxy();
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_.reset(
      new InvalidationNotifier(notifier_options, client_info));
  invalidation_notifier_->AddObserver(this);
}


void NonBlockingInvalidationNotifier::Core::Teardown() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_->RemoveObserver(this);
  invalidation_notifier_.reset();
  io_message_loop_proxy_ = NULL;
}

void NonBlockingInvalidationNotifier::Core::AddObserver(
    SyncNotifierObserver* observer) {
  observers_->AddObserver(observer);
}

void NonBlockingInvalidationNotifier::Core::RemoveObserver(
    SyncNotifierObserver* observer) {
  observers_->RemoveObserver(observer);
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
    const syncable::ModelTypeSet& types) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateEnabledTypes(types);
}

void NonBlockingInvalidationNotifier::Core::OnIncomingNotification(
        const syncable::ModelTypePayloadMap& type_payloads) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  observers_->Notify(&SyncNotifierObserver::OnIncomingNotification,
                     type_payloads);
}

void NonBlockingInvalidationNotifier::Core::OnNotificationStateChange(
        bool notifications_enabled) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  observers_->Notify(&SyncNotifierObserver::OnNotificationStateChange,
                     notifications_enabled);
}

void NonBlockingInvalidationNotifier::Core::StoreState(
    const std::string& state) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  observers_->Notify(&SyncNotifierObserver::StoreState, state);
}

NonBlockingInvalidationNotifier::NonBlockingInvalidationNotifier(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info)
        : core_(new Core),
          construction_message_loop_proxy_(
              base::MessageLoopProxy::CreateForCurrentThread()),
          io_message_loop_proxy_(notifier_options.request_context_getter->
              GetIOMessageLoopProxy()) {
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          core_.get(),
          &NonBlockingInvalidationNotifier::Core::Initialize,
          notifier_options, client_info));
}

NonBlockingInvalidationNotifier::~NonBlockingInvalidationNotifier() {
  DCHECK(construction_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          core_.get(),
          &NonBlockingInvalidationNotifier::Core::Teardown));
}

void NonBlockingInvalidationNotifier::AddObserver(
    SyncNotifierObserver* observer) {
  CheckOrSetValidThread();
  core_->AddObserver(observer);
}

void NonBlockingInvalidationNotifier::RemoveObserver(
    SyncNotifierObserver* observer) {
  CheckOrSetValidThread();
  core_->RemoveObserver(observer);
}

void NonBlockingInvalidationNotifier::SetState(const std::string& state) {
  CheckOrSetValidThread();
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          core_.get(),
          &NonBlockingInvalidationNotifier::Core::SetState,
          state));
}

void NonBlockingInvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  CheckOrSetValidThread();
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          core_.get(),
          &NonBlockingInvalidationNotifier::Core::UpdateCredentials,
          email, token));
}

void NonBlockingInvalidationNotifier::UpdateEnabledTypes(
    const syncable::ModelTypeSet& types) {
  CheckOrSetValidThread();
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          core_.get(),
          &NonBlockingInvalidationNotifier::Core::UpdateEnabledTypes,
          types));
}

void NonBlockingInvalidationNotifier::SendNotification() {
  CheckOrSetValidThread();
  // InvalidationClient doesn't implement SendNotification(), so no
  // need to forward on the call.
}

void NonBlockingInvalidationNotifier::CheckOrSetValidThread() {
  if (method_message_loop_proxy_) {
    DCHECK(method_message_loop_proxy_->BelongsToCurrentThread());
  } else {
    method_message_loop_proxy_ =
        base::MessageLoopProxy::CreateForCurrentThread();
  }
}

}  // namespace sync_notifier
