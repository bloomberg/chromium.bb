// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYNCED_NOTIFICATIONS_PRIVATE_SYNCED_NOTIFICATIONS_SHIM_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYNCED_NOTIFICATIONS_PRIVATE_SYNCED_NOTIFICATIONS_SHIM_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/synced_notifications_private.h"
#include "sync/api/syncable_service.h"

namespace extensions {
struct Event;
}

// Shim for SYNCED_NOTIFICATIONS and SYNCED_NOTIFICATIONS_APP_INFO datatypes
// to interface with their extension backed sync code.
class SyncedNotificationsShim : public syncer::SyncableService {
 public:
  // Callback for firing extension events.
  typedef base::Callback<void(scoped_ptr<extensions::Event>)> EventLauncher;

  explicit SyncedNotificationsShim(const EventLauncher& event_launcher,
                                   const base::Closure& refresh_request);
  virtual ~SyncedNotificationsShim();

  // SyncableService interface.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;

  // JS interface methods (see synced_notifications_private.h).
  bool GetInitialData(
      extensions::api::synced_notifications_private::SyncDataType data_type,
      std::vector<
          linked_ptr<extensions::api::synced_notifications_private::SyncData> >*
          js_data_list) const;
  bool UpdateNotification(const std::string& changed_notification);
  bool SetRenderContext(
      extensions::api::synced_notifications_private::RefreshRequest
          refresh_status,
      const std::string& new_context);

  // Returns whether both sync notification datatypes are up and running.
  bool IsSyncReady() const;

 private:
  // Callback to trigger firing extension events.
  EventLauncher event_launcher_;

  // Callback to trigger synced notification refresh.
  base::Closure refresh_request_;

  // The sync change processors, initialized via MergeDataAndStartSyncing.
  scoped_ptr<syncer::SyncChangeProcessor> notifications_change_processor_;
  scoped_ptr<syncer::SyncChangeProcessor> app_info_change_processor_;

  DISALLOW_COPY_AND_ASSIGN(SyncedNotificationsShim);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYNCED_NOTIFICATIONS_PRIVATE_SYNCED_NOTIFICATIONS_SHIM_H_
