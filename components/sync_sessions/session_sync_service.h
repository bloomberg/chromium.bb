// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/model/model_type_store.h"
#include "components/version_info/channel.h"

namespace syncer {
class GlobalIdMapper;
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace sync_sessions {

class FaviconCache;
class OpenTabsUIDelegate;
class SessionSyncBridge;
class SyncSessionsClient;

// KeyedService responsible session sync (aka tab sync), including favicon sync.
// This powers things like the history UI, where "Tabs from other devices"
// exists, as well as the uploading counterpart for other devices to see our
// local tabs.
class SessionSyncService : public KeyedService {
 public:
  SessionSyncService(version_info::Channel channel,
                     std::unique_ptr<SyncSessionsClient> sessions_client);
  ~SessionSyncService() override;

  syncer::GlobalIdMapper* GetGlobalIdMapper() const;

  // Return the active OpenTabsUIDelegate. If open/proxy tabs is not enabled or
  // not currently syncing, returns nullptr.
  OpenTabsUIDelegate* GetOpenTabsUIDelegate();

  // Allows client code to be notified when foreign sessions change.
  std::unique_ptr<base::CallbackList<void()>::Subscription>
  SubscribeToForeignSessionsChanged(const base::RepeatingClosure& cb)
      WARN_UNUSED_RESULT;

  // Schedules garbage collection of foreign sessions.
  void ScheduleGarbageCollection();

  // For ProfileSyncService to initialize the controller for SESSIONS.
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate();

  // For ProfileSyncService to initialize the controller for FAVICON_IMAGES and
  // FAVICON_TRACKING.
  FaviconCache* GetFaviconCache();

  // Intended to be used by ProxyDataTypeController: influences whether
  // GetOpenTabsUIDelegate() returns null or not.
  void ProxyTabsStateChanged(syncer::DataTypeController::State state);

  // Used on Android only, to override the machine tag.
  void SetSyncSessionsGUID(const std::string& guid);

  // Returns OpenTabsUIDelegate regardless of sync being enabled or disabled,
  // useful for tests.
  OpenTabsUIDelegate* GetUnderlyingOpenTabsUIDelegateForTest();

 private:
  void NotifyForeignSessionUpdated();

  std::unique_ptr<SyncSessionsClient> sessions_client_;

  bool proxy_tabs_running_ = false;

  std::unique_ptr<SessionSyncBridge> bridge_;

  base::CallbackList<void()> foreign_sessions_changed_callback_list_;

  DISALLOW_COPY_AND_ASSIGN(SessionSyncService);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_
