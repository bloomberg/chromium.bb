// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_STORAGE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_STORAGE_MANAGER_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/arc/storage_manager/arc_storage_manager.h"

namespace chromeos {
namespace options {

// Storage manager overlay page UI handler.
class StorageManagerHandler : public ::options::OptionsPageUIHandler {
 public:
  // Enumeration for device state about remaining space. These values must be
  // kept in sync with options.StorageSpaceState in JS code.
  enum StorageSpaceState {
    STORAGE_SPACE_NORMAL = 0,
    STORAGE_SPACE_LOW = 1,
    STORAGE_SPACE_CRITICALLY_LOW = 2,
  };

  StorageManagerHandler();
  ~StorageManagerHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializePage() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Handlers of JS messages.
  void HandleUpdateStorageInfo(const base::ListValue* unused_args);
  void HandleOpenDownloads(const base::ListValue* unused_args);
  void HandleOpenArcStorage(const base::ListValue* unused_args);
  void HandleClearDriveCache(const base::ListValue* unused_args);

  // Requests updating disk space information.
  void UpdateSizeStat();

  // Callback to update the UI about disk space information.
  void OnGetSizeStat(int64_t* total_size, int64_t* available_size);

  // Requests updating the size of Downloads directory.
  void UpdateDownloadsSize();

  // Callback to update the UI about the size of Downloads directory.
  void OnGetDownloadsSize(int64_t size);

  // Requests updating the size of Drive Cache.
  void UpdateDriveCacheSize();

  // Callback to update the UI about the size of Drive Cache.
  void OnGetDriveCacheSize(int64_t size);

  // Requests updating the size of browsing data.
  void UpdateBrowsingDataSize();

  // Callback to update the UI about the size of browsing data.
  void OnGetBrowsingDataSize(bool is_site_data, int64_t size);

  // Requests updating the space size used by Android apps and cache.
  void UpdateArcSize();

  // Callback to update the UI about Android apps and cache.
  void OnGetArcSize(bool succeeded, arc::mojom::ApplicationsSizePtr size);

  // Callback called when clearing Drive cache is done.
  void OnClearDriveCacheDone(bool success);

  // Total size of cache data in browsing data.
  int64_t browser_cache_size_;

  // Total size of site data in browsing data.
  int64_t browser_site_data_size_;

  base::WeakPtrFactory<StorageManagerHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StorageManagerHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_STORAGE_MANAGER_HANDLER_H_
