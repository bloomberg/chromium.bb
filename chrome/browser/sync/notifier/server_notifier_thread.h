// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is the (hackish) way to use the XMPP parts of
// MediatorThread for server-issued notifications.
//
// TODO(akalin): Separate this code from notifier::MediatorThreadImpl
// and combine it with InvalidationNotifier.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_SERVER_NOTIFIER_THREAD_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_SERVER_NOTIFIER_THREAD_H_
#pragma once

#include <string>
#include <vector>

#include "base/observer_list_threadsafe.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "jingle/notifier/listener/mediator_thread_impl.h"

namespace notifier {
struct NotifierOptions;
}

namespace sync_notifier {

class ServerNotifierThread
    : public notifier::MediatorThreadImpl,
      public ChromeInvalidationClient::Listener,
      public StateWriter {
 public:
  // Does not take ownership of |state_writer| (which may not
  // be NULL).
  explicit ServerNotifierThread(
      const notifier::NotifierOptions& notifier_options,
      const std::string& client_info,
      StateWriter* state_writer);

  virtual ~ServerNotifierThread();

  // Should be called exactly once before Login().
  void SetState(const std::string& state);

  // Must be called after Start() is called and before Logout() is
  // called.
  void UpdateEnabledTypes(const syncable::ModelTypeSet& types);

  // Overridden to stop the invalidation listener.
  virtual void Logout();

  // Overridden to do actual login.
  virtual void OnConnect(base::WeakPtr<talk_base::Task> base_task);

  // ChromeInvalidationClient::Listener implementation.
  // We pass on two pieces of information to observers through the
  // Notification.
  // - the model type being invalidated, through the Notification's
  //       |channel| member.
  // - the invalidation payload for that model type, through the
  //       Notification's |data| member.
  //
  // TODO(akalin): Remove this hack once we merge with
  // InvalidationNotifier.
  virtual void OnInvalidate(syncable::ModelType model_type,
                            const std::string& payload);
  virtual void OnInvalidateAll();

  // StateWriter implementation.
  virtual void WriteState(const std::string& state);

 private:
  // Made private because this function shouldn't be called.
  virtual void SendNotification(const notifier::Notification& data);

  // Posted to the worker thread by UpdateEnabledTypes().
  void DoUpdateEnabledTypes(const syncable::ModelTypeSet& types);

  // Posted to the worker thread by UpdateEnabledTypes() and also
  // called by OnConnect().
  void RegisterEnabledTypes();

  // Posted to the worker thread by Logout().
  void StopInvalidationListener();

  const std::string client_info_;
  // Hack to get the nice thread-safe behavior for |state_writer_|.
  scoped_refptr<ObserverListThreadSafe<StateWriter> > state_writers_;
  // We still need to keep |state_writer_| around to remove it from
  // |state_writers_|.
  StateWriter* state_writer_;
  scoped_ptr<ChromeInvalidationClient> chrome_invalidation_client_;

  // The state to pass to |chrome_invalidation_client_|.
  std::string state_;

  // The types to register.  Should only be accessed on the worker
  // thread.
  syncable::ModelTypeSet enabled_types_;
};

}  // namespace sync_notifier

DISABLE_RUNNABLE_METHOD_REFCOUNT(sync_notifier::ServerNotifierThread);

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_SERVER_NOTIFIER_THREAD_H_
