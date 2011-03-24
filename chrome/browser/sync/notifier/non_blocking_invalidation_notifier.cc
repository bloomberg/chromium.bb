// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/non_blocking_invalidation_notifier.h"

#include "base/logging.h"
#include "base/message_loop.h"

namespace sync_notifier {

NonBlockingInvalidationNotifier::NonBlockingInvalidationNotifier(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info)
    : parent_message_loop_(MessageLoop::current()),
      observers_(new ObserverListThreadSafe<SyncNotifierObserver>()),
      worker_thread_("InvalidationNotifier worker thread"),
      worker_thread_vars_(NULL) {
  DCHECK_EQ(notifier::NOTIFICATION_SERVER,
            notifier_options.notification_method);
  const base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  CHECK(worker_thread_.StartWithOptions(options));
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &NonBlockingInvalidationNotifier::CreateWorkerThreadVars,
          notifier_options, client_info));
}

NonBlockingInvalidationNotifier::~NonBlockingInvalidationNotifier() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &NonBlockingInvalidationNotifier::DestroyWorkerThreadVars));
  worker_thread_.Stop();
  CHECK(!worker_thread_vars_);
}

void NonBlockingInvalidationNotifier::AddObserver(
    SyncNotifierObserver* observer) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  observers_->AddObserver(observer);
}

void NonBlockingInvalidationNotifier::RemoveObserver(
    SyncNotifierObserver* observer) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  observers_->RemoveObserver(observer);
}

void NonBlockingInvalidationNotifier::SetState(const std::string& state) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &NonBlockingInvalidationNotifier::SetStateOnWorkerThread,
          state));
}

void NonBlockingInvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &NonBlockingInvalidationNotifier::UpdateCredentialsOnWorkerThread,
          email, token));
}

void NonBlockingInvalidationNotifier::UpdateEnabledTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &NonBlockingInvalidationNotifier::UpdateEnabledTypesOnWorkerThread,
          types));
}

void NonBlockingInvalidationNotifier::SendNotification() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  // InvalidationClient doesn't implement SendNotification(), so no
  // need to forward on the call.
}

MessageLoop* NonBlockingInvalidationNotifier::worker_message_loop() {
  MessageLoop* current_message_loop = MessageLoop::current();
  DCHECK(current_message_loop);
  MessageLoop* worker_message_loop = worker_thread_.message_loop();
  DCHECK(worker_message_loop);
  DCHECK(current_message_loop == parent_message_loop_ ||
         current_message_loop == worker_message_loop);
  return worker_message_loop;
}

void NonBlockingInvalidationNotifier::CreateWorkerThreadVars(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  worker_thread_vars_ =
      new WorkerThreadVars(notifier_options, client_info, observers_);
}

void NonBlockingInvalidationNotifier::DestroyWorkerThreadVars() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  delete worker_thread_vars_;
  worker_thread_vars_ = NULL;
}

void NonBlockingInvalidationNotifier::SetStateOnWorkerThread(
    const std::string& state) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  worker_thread_vars_->invalidation_notifier.SetState(state);
}

void NonBlockingInvalidationNotifier::UpdateCredentialsOnWorkerThread(
    const std::string& email, const std::string& token) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  worker_thread_vars_->invalidation_notifier.UpdateCredentials(email, token);
}

void NonBlockingInvalidationNotifier::UpdateEnabledTypesOnWorkerThread(
    const syncable::ModelTypeSet& types) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  worker_thread_vars_->invalidation_notifier.UpdateEnabledTypes(types);
}

NonBlockingInvalidationNotifier::ObserverRouter::ObserverRouter(
    const scoped_refptr<ObserverListThreadSafe<SyncNotifierObserver> >&
        observers) : observers_(observers) {}

NonBlockingInvalidationNotifier::ObserverRouter::~ObserverRouter() {}

void NonBlockingInvalidationNotifier::ObserverRouter::
    OnIncomingNotification(
        const syncable::ModelTypePayloadMap& type_payloads) {
  observers_->Notify(&SyncNotifierObserver::OnIncomingNotification,
                     type_payloads);
}

void NonBlockingInvalidationNotifier::ObserverRouter::
    OnNotificationStateChange(
        bool notifications_enabled) {
  observers_->Notify(&SyncNotifierObserver::OnNotificationStateChange,
                     notifications_enabled);
}

void NonBlockingInvalidationNotifier::ObserverRouter::StoreState(
    const std::string& state) {
  observers_->Notify(&SyncNotifierObserver::StoreState, state);
}

NonBlockingInvalidationNotifier::WorkerThreadVars::WorkerThreadVars(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info,
    const scoped_refptr<ObserverListThreadSafe<SyncNotifierObserver> >&
        observers)
    : host_resolver_(
        net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                      NULL, NULL)),
      invalidation_notifier(notifier_options, host_resolver_.get(),
                            &cert_verifier_, client_info),
      observer_router_(observers) {
  invalidation_notifier.AddObserver(&observer_router_);
}

NonBlockingInvalidationNotifier::WorkerThreadVars::~WorkerThreadVars() {
  invalidation_notifier.RemoveObserver(&observer_router_);
}

}  // namespace sync_notifier
