// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SyncSessionContext encapsulates the contextual information and engine
// components specific to a SyncSession. A context is accessible via
// a SyncSession so that session SyncerCommands and parts of the engine have
// a convenient way to access other parts. In this way it can be thought of as
// the surrounding environment for the SyncSession. The components of this
// environment are either valid or not valid for the entire context lifetime,
// or they are valid for explicitly scoped periods of time by using Scoped
// installation utilities found below. This means that the context assumes no
// ownership whatsoever of any object that was not created by the context
// itself.
//
// It can only be used from the SyncerThread.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/syncer_types.h"

namespace syncable {
class DirectoryManager;
}

namespace browser_sync {

class ConflictResolver;
class ExtensionsActivityMonitor;
class ModelSafeWorkerRegistrar;
class ServerConnectionManager;

// Default number of items a client can commit in a single message.
static const int kDefaultMaxCommitBatchSize = 25;

namespace sessions {
class ScopedSessionContextConflictResolver;
struct SyncSessionSnapshot;
class TestScopedSessionEventListener;

class SyncSessionContext {
 public:
  SyncSessionContext(ServerConnectionManager* connection_manager,
                     syncable::DirectoryManager* directory_manager,
                     ModelSafeWorkerRegistrar* model_safe_worker_registrar,
                     const std::vector<SyncEngineEventListener*>& listeners);
  ~SyncSessionContext();

  ConflictResolver* resolver() { return resolver_; }
  ServerConnectionManager* connection_manager() {
    return connection_manager_;
  }
  syncable::DirectoryManager* directory_manager() {
    return directory_manager_;
  }
  ModelSafeWorkerRegistrar* registrar() {
    return registrar_;
  }
  ExtensionsActivityMonitor* extensions_monitor() {
    return extensions_activity_monitor_;
  }

  // Talk notification status.
  void set_notifications_enabled(bool enabled) {
    notifications_enabled_ = enabled;
  }
  bool notifications_enabled() { return notifications_enabled_; }

  // Account name, set once a directory has been opened.
  void set_account_name(const std::string name) {
    DCHECK(account_name_.empty());
    account_name_ = name;
  }
  const std::string& account_name() { return account_name_; }

  void set_max_commit_batch_size(int batch_size) {
    max_commit_batch_size_ = batch_size;
  }
  int32 max_commit_batch_size() const { return max_commit_batch_size_; }

  const ModelSafeRoutingInfo& previous_session_routing_info() const {
    return previous_session_routing_info_;
  }

  void set_previous_session_routing_info(const ModelSafeRoutingInfo& info) {
    previous_session_routing_info_ = info;
  }

  void NotifyListeners(const SyncEngineEvent& event) {
    FOR_EACH_OBSERVER(SyncEngineEventListener, listeners_,
                      OnSyncEngineEvent(event));
  }

 private:
  // Rather than force clients to set and null-out various context members, we
  // extend our encapsulation boundary to scoped helpers that take care of this
  // once they are allocated. See definitions of these below.
  friend class ScopedSessionContextConflictResolver;
  friend class TestScopedSessionEventListener;

  // This is installed by Syncer objects when needed and may be NULL.
  ConflictResolver* resolver_;

  ObserverList<SyncEngineEventListener> listeners_;

  ServerConnectionManager* const connection_manager_;
  syncable::DirectoryManager* const directory_manager_;

  // A registrar of workers capable of processing work closures on a thread
  // that is guaranteed to be safe for model modifications.
  ModelSafeWorkerRegistrar* registrar_;

  // We use this to stuff extensions activity into CommitMessages so the server
  // can correlate commit traffic with extension-related bookmark mutations.
  ExtensionsActivityMonitor* extensions_activity_monitor_;

  // Kept up to date with talk events to determine whether notifications are
  // enabled. True only if the notification channel is authorized and open.
  bool notifications_enabled_;

  // The name of the account being synced.
  std::string account_name_;

  // The server limits the number of items a client can commit in one batch.
  int max_commit_batch_size_;

  // Some routing info history to help us clean up types that get disabled
  // by the user.
  ModelSafeRoutingInfo previous_session_routing_info_;

  // Cache of last session snapshot information.
  scoped_ptr<sessions::SyncSessionSnapshot> previous_session_snapshot_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionContext);
};

// Installs a ConflictResolver to a given session context for the lifetime of
// the ScopedSessionContextConflictResolver.  There should never be more than
// one ConflictResolver in the system, so it is an error to use this if the
// context already has a resolver.
class ScopedSessionContextConflictResolver {
 public:
  // Note: |context| and |resolver| should outlive |this|.
  ScopedSessionContextConflictResolver(SyncSessionContext* context,
                                       ConflictResolver* resolver)
      : context_(context), resolver_(resolver) {
    DCHECK(NULL == context->resolver_);
    context->resolver_ = resolver;
  }
  ~ScopedSessionContextConflictResolver() {
    context_->resolver_ = NULL;
   }
 private:
  SyncSessionContext* context_;
  ConflictResolver* resolver_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSessionContextConflictResolver);
};

}  // namespace sessions
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
