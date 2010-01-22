// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

#include <string>
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/util/extensions_activity_monitor.h"

namespace syncable {
class DirectoryManager;
}

namespace browser_sync {

class ConflictResolver;
class ModelSafeWorkerRegistrar;
class ServerConnectionManager;

namespace sessions {
class ScopedSessionContextConflictResolver;
class ScopedSessionContextSyncerEventChannel;

class SyncSessionContext {
 public:
  SyncSessionContext(ServerConnectionManager* connection_manager,
                     syncable::DirectoryManager* directory_manager,
                     ModelSafeWorkerRegistrar* model_safe_worker_registrar)
      : resolver_(NULL),
        syncer_event_channel_(NULL),
        connection_manager_(connection_manager),
        directory_manager_(directory_manager),
        registrar_(model_safe_worker_registrar),
        extensions_activity_monitor_(new ExtensionsActivityMonitor()),
        notifications_enabled_(false) {
  }

  ~SyncSessionContext() {
    // In unittests, there may be no UI thread, so the above will fail.
    if (!ChromeThread::DeleteSoon(ChromeThread::UI, FROM_HERE,
                                  extensions_activity_monitor_)) {
      delete extensions_activity_monitor_;
    }
  }

  ConflictResolver* resolver() { return resolver_; }
  ServerConnectionManager* connection_manager() {
    return connection_manager_;
  }
  syncable::DirectoryManager* directory_manager() {
    return directory_manager_;
  }
  SyncerEventChannel* syncer_event_channel() {
    return syncer_event_channel_;
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

 private:
  // Rather than force clients to set and null-out various context members, we
  // extend our encapsulation boundary to scoped helpers that take care of this
  // once they are allocated. See definitions of these below.
  friend class ScopedSessionContextConflictResolver;
  friend class ScopedSessionContextSyncerEventChannel;

  // These are installed by Syncer objects when needed and may be NULL.
  ConflictResolver* resolver_;
  SyncerEventChannel* syncer_event_channel_;

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

// Installs a SyncerEventChannel to a given session context for the lifetime of
// the ScopedSessionContextSyncerEventChannel.  There should never be more than
// one SyncerEventChannel in the context, so it is an error to use this if the
// context already has a channel.
class ScopedSessionContextSyncerEventChannel {
 public:
  ScopedSessionContextSyncerEventChannel(SyncSessionContext* context,
                                         SyncerEventChannel* channel)
      : context_(context), channel_(channel) {
    DCHECK(NULL == context->syncer_event_channel_);
    context->syncer_event_channel_ = channel_;
  }
  ~ScopedSessionContextSyncerEventChannel() {
    context_->syncer_event_channel_ = NULL;
  }
 private:
  SyncSessionContext* context_;
  SyncerEventChannel* channel_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSessionContextSyncerEventChannel);
};

}  // namespace sessions
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSION_CONTEXT_H_
