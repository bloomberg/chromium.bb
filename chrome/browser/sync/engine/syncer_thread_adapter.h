// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/engine/syncer_thread2.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {

class SyncerThreadAdapter {
 public:
  SyncerThreadAdapter(sessions::SyncSessionContext* context,
                      bool using_new_impl);
  ~SyncerThreadAdapter();

  // We mimic the SyncerThread interface (an adaptee).
  void WatchConnectionManager(ServerConnectionManager* conn_mgr);
  bool Start();
  bool Stop(int max_wait);
  bool RequestPause();
  bool RequestResume();
  void NudgeSyncer(int milliseconds_from_now,
                   SyncerThread::NudgeSource source);
  void NudgeSyncerWithDataTypes(
      int milliseconds_from_now,
      SyncerThread::NudgeSource source,
      const syncable::ModelTypeBitSet& model_types);
  void NudgeSyncerWithPayloads(
      int milliseconds_from_now,
      SyncerThread::NudgeSource source,
      const sessions::TypePayloadMap& model_types_with_payloads);
  void SetNotificationsEnabled(bool enabled);
  void CreateSyncer(const std::string& dirname);

  // Expose the new impl to leverage new apis through the adapter.
  s3::SyncerThread* new_impl();
 private:
  scoped_refptr<SyncerThread> legacy_;
  scoped_ptr<s3::SyncerThread> new_impl_;
  bool using_new_impl_;

  DISALLOW_COPY_AND_ASSIGN(SyncerThreadAdapter);
};

}  //  namespace browser_sync
