// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_INFO_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_INFO_EVENT_ROUTER_H_

#include <set>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "ui/gfx/display_observer.h"

namespace extensions {

namespace api {

namespace experimental_system_info_cpu {

struct CpuUpdateInfo;

}  // namespace experimental_system_info_cpu

}  // namespace api

// Event router for systemInfo API. It is a singleton instance shared by
// multiple profiles.
// TODO(hongbo): It should derive from SystemMonitor::DevicesChangedObserver.
// Since the system_monitor will be refactored along with media_gallery, once
// http://crbug.com/145400 is fixed, we need to update SystemInfoEventRouter
// accordingly.
class SystemInfoEventRouter
    : public gfx::DisplayObserver,
      public StorageInfoObserver {
 public:
  static SystemInfoEventRouter* GetInstance();

  // Add/remove event listener for the |event_name| event from |profile|.
  void AddEventListener(const std::string& event_name);
  void RemoveEventListener(const std::string& event_name);

  // Return true if the |event_name| is an event from systemInfo namespace.
  static bool IsSystemInfoEvent(const std::string& event_name);

  // StorageInfoObserver implementation:
  virtual void OnStorageFreeSpaceChanged(const std::string& id,
                                         double new_value,
                                         double old_value) OVERRIDE;

  // TODO(hongbo): The following methods should be likely overriden from
  // SystemMonitor::DevicesChangedObserver once the http://crbug.com/145400
  // is fixed.
  void OnRemovableStorageAttached(const std::string& id,
                                  const string16& name,
                                  const base::FilePath::StringType& location);
  void OnRemovableStorageDetached(const std::string& id);

  // gfx::DisplayObserver implementation.
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<SystemInfoEventRouter>;

  SystemInfoEventRouter();
  virtual ~SystemInfoEventRouter();

  // Called from any thread to dispatch the systemInfo event to all extension
  // processes cross multiple profiles.
  void DispatchEvent(const std::string& event_name,
      scoped_ptr<base::ListValue> args);

  // The callbacks of querying storage info to start and stop watching the
  // storages. Called from UI thread.
  void StartWatchingStorages(const StorageInfo& info, bool success);
  void StopWatchingStorages(const StorageInfo& info, bool success);

  // The callback for CPU sampling cycle. Called from FILE thread.
  void OnNextCpuSampling(
      scoped_ptr<api::experimental_system_info_cpu::CpuUpdateInfo> info);

  // Called to dispatch the systemInfo.display.onDisplayChanged event.
  void OnDisplayChanged();

  // Used to record the event names being watched.
  std::multiset<std::string> watching_event_set_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_INFO_EVENT_ROUTER_H_
