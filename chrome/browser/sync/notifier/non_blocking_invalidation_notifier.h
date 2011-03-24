// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of SyncNotifier that wraps InvalidationNotifier
// on its own thread.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/observer_list_threadsafe.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/notifier/invalidation_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"

class MessageLoop;

namespace sync_notifier {

class NonBlockingInvalidationNotifier : public SyncNotifier {
 public:
  NonBlockingInvalidationNotifier(
      const notifier::NotifierOptions& notifier_options,
      const std::string& client_info);

  virtual ~NonBlockingInvalidationNotifier();

  // SyncNotifier implementation.
  virtual void AddObserver(SyncNotifierObserver* observer);
  virtual void RemoveObserver(SyncNotifierObserver* observer);
  virtual void SetState(const std::string& state);
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token);
  virtual void UpdateEnabledTypes(const syncable::ModelTypeSet& types);
  virtual void SendNotification();

 private:
  // Utility class that routes received notifications to a given
  // thread-safe observer list.
  class ObserverRouter : public SyncNotifierObserver {
   public:
    explicit ObserverRouter(
        const scoped_refptr<ObserverListThreadSafe<SyncNotifierObserver> >&
            observers);

    virtual ~ObserverRouter();

    // SyncNotifierObserver implementation.
    virtual void OnIncomingNotification(
        const syncable::ModelTypePayloadMap& type_payloads);
    virtual void OnNotificationStateChange(bool notifications_enabled);
    virtual void StoreState(const std::string& state);

   private:
    scoped_refptr<ObserverListThreadSafe<SyncNotifierObserver> > observers_;
  };

  // The set of variables that should only be created/used on the
  // worker thread.
  struct WorkerThreadVars {
    WorkerThreadVars(
        const notifier::NotifierOptions& notifier_options,
        const std::string& client_info,
        const scoped_refptr<ObserverListThreadSafe<SyncNotifierObserver> >&
            observers);
    ~WorkerThreadVars();

   private:
    scoped_ptr<net::HostResolver> host_resolver_;
    net::CertVerifier cert_verifier_;

   public:
    // This needs to be initialized after |host_resolver_| and
    // |cert_verifier_|.
    InvalidationNotifier invalidation_notifier;

   private:
    ObserverRouter observer_router_;

    DISALLOW_COPY_AND_ASSIGN(WorkerThreadVars);
  };

  MessageLoop* worker_message_loop();

  void CreateWorkerThreadVars(
      const notifier::NotifierOptions& notifier_options,
      const std::string& client_info);
  void DestroyWorkerThreadVars();

  // Equivalents of the public functions that are run on the worker
  // thread.
  void SetStateOnWorkerThread(const std::string& state);
  void UpdateCredentialsOnWorkerThread(const std::string& email,
                                       const std::string& token);
  void UpdateEnabledTypesOnWorkerThread(const syncable::ModelTypeSet& types);

  MessageLoop* parent_message_loop_;

  scoped_refptr<ObserverListThreadSafe<SyncNotifierObserver> > observers_;

  base::Thread worker_thread_;
  // Created and destroyed on the worker thread.  Not a scoped_ptr as
  // it's better to leak memory than to delete on the wrong thread.
  // Created by CreateWorkerThreadVars() and destroyed by
  // DestroyWorkerThreadVars().
  WorkerThreadVars* worker_thread_vars_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingInvalidationNotifier);
};

}  // namespace sync_notifier

// We own our worker thread, so we don't need to be ref-counted.
DISABLE_RUNNABLE_METHOD_REFCOUNT(
    sync_notifier::NonBlockingInvalidationNotifier);

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_
