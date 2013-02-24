// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_PROVIDER_LINUX_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_PROVIDER_LINUX_H_

#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/browser/storage_monitor/udev_util_linux.h"

namespace extensions {

class StorageInfoProviderLinux : public StorageInfoProvider {
 public:
  StorageInfoProviderLinux();

 protected:
  virtual ~StorageInfoProviderLinux();

  // For unit test.
  explicit StorageInfoProviderLinux(const base::FilePath& mtab_path);

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE;

  virtual bool QueryUnitInfo(const std::string& mount_path,
      api::experimental_system_info_storage::StorageUnitInfo* info) OVERRIDE;

  // Query the storage type for the given |mount_path|. The returned value is
  // placed in |type|. Return false if the |mount_path| is not found.
  virtual bool QueryStorageType(const std::string& mount_path,
                                std::string* type);

  // The udev context for querying device information.
  chrome::ScopedUdevObject udev_context_;

  // The mtab file path on the system.
  const base::FilePath mtab_file_path_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_PROVIDER_LINUX_H_
