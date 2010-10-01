// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session_context.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/util/extensions_activity_monitor.h"
#include "chrome/browser/sync/sessions/session_state.h"

namespace browser_sync {
namespace sessions {

SyncSessionContext::SyncSessionContext(
    ServerConnectionManager* connection_manager,
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

SyncSessionContext::~SyncSessionContext() {
  // In unittests, there may be no UI thread, so the above will fail.
  if (!ChromeThread::DeleteSoon(ChromeThread::UI, FROM_HERE,
                                extensions_activity_monitor_)) {
    delete extensions_activity_monitor_;
  }
}

void SyncSessionContext::set_last_snapshot(
    const SyncSessionSnapshot& snapshot) {
  previous_session_snapshot_.reset(new SyncSessionSnapshot(snapshot));
}

}  // namespace sessions
}  // namespace browser_sync
