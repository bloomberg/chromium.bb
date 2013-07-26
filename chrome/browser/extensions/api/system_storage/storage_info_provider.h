// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/browser/extensions/api/system_storage/storage_free_space_observer.h"
#include "chrome/browser/storage_monitor/removable_storage_observer.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/common/extensions/api/system_storage.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {

namespace systeminfo {

extern const char kStorageTypeUnknown[];
extern const char kStorageTypeFixed[];
extern const char kStorageTypeRemovable[];

// Build StorageUnitInfo struct from chrome::StorageInfo instance. The |unit|
// parameter is the output value.
void BuildStorageUnitInfo(const chrome::StorageInfo& info,
    api::system_storage::StorageUnitInfo* unit);

}  // namespace systeminfo

typedef std::vector<linked_ptr<
    api::system_storage::StorageUnitInfo> >
        StorageUnitInfoList;

class StorageInfoProvider : public SystemInfoProvider<StorageUnitInfoList> {
 public:
  StorageInfoProvider();

  // Get the single shared instance of StorageInfoProvider.
  static StorageInfoProvider* Get();

  // Add and remove observer, both can be called from any thread.
  void AddObserver(StorageFreeSpaceObserver* obs);
  void RemoveObserver(StorageFreeSpaceObserver* obs);

  // Start and stop watching the given storage |transient_id|.
  void StartWatching(const std::string& transient_id);
  void StopWatching(const std::string& transient_id);

  // Start and stop watching all available storages.
  void StartWatchingAllStorages();
  void StopWatchingAllStorages();

  // Returns all available storages, including fixed and removable.
  virtual std::vector<chrome::StorageInfo> GetAllStorages() const;

  // SystemInfoProvider implementations
  virtual void PrepareQueryOnUIThread() OVERRIDE;
  virtual void InitializeProvider(const base::Closure& do_query_info_callback)
      OVERRIDE;

  // Get the amount of storage free space from |transient_id|, or -1 on failure.
  virtual int64 GetStorageFreeSpaceFromTransientId(
      const std::string& transient_id);

  virtual std::string GetTransientIdForDeviceId(
      const std::string& device_id) const;
  virtual std::string GetDeviceIdForTransientId(
      const std::string& transient_id) const;

  const StorageUnitInfoList& storage_unit_info_list() const;

 protected:
  virtual ~StorageInfoProvider();

  // TODO(Haojian): Put this method in a testing subclass rather than here.
  void SetWatchingIntervalForTesting(size_t ms) { watching_interval_ = ms; }

  // Put all available storages' information into |info_|.
  virtual void GetAllStoragesIntoInfoList();

 private:
  typedef std::map<std::string, double> StorageTransientIdToSizeMap;

  // SystemInfoProvider implementations.
  // Override to query the available capacity of all known storage devices on
  // the blocking pool, including fixed and removable devices.
  virtual bool QueryInfo() OVERRIDE;

  // Query the new attached removable storage info on the blocking pool.
  void QueryAttachedStorageInfoOnBlockingPool(const std::string& transient_id);

  // Posts a task to check for free space changes on the blocking pool.
  // Should be called on the UI thread.
  void CheckWatchedStorages();

  // Check if the free space changes for the watched storages by iterating over
  // the |storage_transient_id_to_size_map_|. It is called on blocking pool.
  void CheckWatchedStorageOnBlockingPool(const std::string& transient_id);

  // Add the storage identified by |transient_id| into watching list.
  void AddWatchedStorageOnBlockingPool(const std::string& transient_id);
  // Remove the storage identified by |transient_id| from watching list.
  void RemoveWatchedStorageOnBlockingPool(const std::string& transient_id);

  void StartWatchingTimerOnUIThread();
  // Force to stop the watching timer or there is no any one storage to be
  // watched. It is called on UI thread.
  void StopWatchingTimerOnUIThread();

  // Mapping of the storage being watched and the recent free space value. It
  // is maintained on the blocking pool.
  StorageTransientIdToSizeMap storage_transient_id_to_size_map_;

  // The timer used for watching the storage free space changes periodically.
  base::RepeatingTimer<StorageInfoProvider> watching_timer_;

  // The thread-safe observer list that observe the changes happening on the
  // storages.
  scoped_refptr<ObserverListThreadSafe<StorageFreeSpaceObserver> > observers_;

  // The time interval for watching the free space change, in milliseconds.
  // Only changed for testing purposes.
  size_t watching_interval_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_
