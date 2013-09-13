// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_

#include <set>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/browser/storage_monitor/removable_storage_observer.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/common/extensions/api/system_storage.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {

namespace systeminfo {

// Build StorageUnitInfo struct from StorageInfo instance. The |unit|
// parameter is the output value.
void BuildStorageUnitInfo(const StorageInfo& info,
                          api::system_storage::StorageUnitInfo* unit);

}  // namespace systeminfo

typedef std::vector<linked_ptr<
    api::system_storage::StorageUnitInfo> >
        StorageUnitInfoList;

class StorageInfoProvider : public SystemInfoProvider {
 public:
  // Get the single shared instance of StorageInfoProvider.
  static StorageInfoProvider* Get();

  // SystemInfoProvider implementations
  virtual void PrepareQueryOnUIThread() OVERRIDE;
  virtual void InitializeProvider(const base::Closure& do_query_info_callback)
      OVERRIDE;

  const StorageUnitInfoList& storage_unit_info_list() const;

  static void InitializeForTesting(scoped_refptr<StorageInfoProvider> provider);

 protected:
  StorageInfoProvider();

  virtual ~StorageInfoProvider();

  // Put all available storages' information into |info_|.
  void GetAllStoragesIntoInfoList();

  // The last information filled up by QueryInfo and is accessed on multiple
  // threads, but the whole class is being guarded by SystemInfoProvider base
  // class.
  //
  // |info_| is accessed on the UI thread while |is_waiting_for_completion_| is
  // false and on the sequenced worker pool while |is_waiting_for_completion_|
  // is true.
  StorageUnitInfoList info_;

 private:
  // SystemInfoProvider implementations.
  // Override to query the available capacity of all known storage devices on
  // the blocking pool, including fixed and removable devices.
  virtual bool QueryInfo() OVERRIDE;

  static base::LazyInstance<scoped_refptr<StorageInfoProvider> > provider_;

  DISALLOW_COPY_AND_ASSIGN(StorageInfoProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_
