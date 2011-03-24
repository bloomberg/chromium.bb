// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of SyncNotifier that wraps an invalidation
// client.  Handles the details of connecting to XMPP and hooking it
// up to the invalidation client.
//
// You probably don't want to use this directly; use
// NonBlockingInvalidationNotifier.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/communicator/login.h"
#include "net/base/cert_verifier.h"

namespace net {
class HostResolver;
class CertVerifier;
}  // namespace net

namespace sync_notifier {

class InvalidationNotifier
    : public SyncNotifier,
      public notifier::LoginDelegate,
      public ChromeInvalidationClient::Listener,
      public StateWriter {
 public:
  // Does not take ownership of |host_resolver| or |cert_verifier|.
  InvalidationNotifier(
      const notifier::NotifierOptions& notifier_options,
      net::HostResolver* host_resolver,
      net::CertVerifier* cert_verifier,
      const std::string& client_info);

  virtual ~InvalidationNotifier();

  // SyncNotifier implementation.
  virtual void AddObserver(SyncNotifierObserver* observer);
  virtual void RemoveObserver(SyncNotifierObserver* observer);
  virtual void SetState(const std::string& state);
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token);
  virtual void UpdateEnabledTypes(const syncable::ModelTypeSet& types);
  virtual void SendNotification();

  // notifier::LoginDelegate implementation.
  virtual void OnConnect(base::WeakPtr<talk_base::Task> base_task);
  virtual void OnDisconnect();

  // ChromeInvalidationClient::Listener implementation.
  virtual void OnInvalidate(syncable::ModelType model_type,
                            const std::string& payload);
  virtual void OnInvalidateAll();

  // StateWriter implementation.
  virtual void WriteState(const std::string& state);

 private:
  void EmitInvalidation(const syncable::ModelTypeSet& types,
                        const std::string& payload);

  base::NonThreadSafe non_thread_safe_;

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

  // Used to build parameters for |login_|.
  const notifier::NotifierOptions notifier_options_;
  net::HostResolver* const host_resolver_;
  net::CertVerifier* const cert_verifier_;

  // Passed to |invalidation_client_|.
  const std::string client_info_;

  // Our observers (which must live on the same thread).
  ObserverList<SyncNotifierObserver> observers_;

  // The state to pass to |chrome_invalidation_client_|.
  std::string invalidation_state_;

  // The XMPP connection manager.
  scoped_ptr<notifier::Login> login_;

  // The invalidation client.
  ChromeInvalidationClient invalidation_client_;

  // Passed to |invalidation_client_| and also used to synthesize
  // notifications by OnInvalidateAll().
  syncable::ModelTypeSet enabled_types_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationNotifier);
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
