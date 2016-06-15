// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/storage_manager_handler.h"

#include <string>

#include "base/files/file_util.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace chromeos {
namespace options {
namespace {

void GetSizeStatOnBlockingPool(const base::FilePath& mount_path,
                               int64_t* total_size,
                               int64_t* available_size) {
  int64_t size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  if (size >= 0)
    *total_size = size;
  size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  if (size >= 0)
    *available_size = size;
}

}  // namespace

StorageManagerHandler::StorageManagerHandler() : weak_ptr_factory_(this) {
}

StorageManagerHandler::~StorageManagerHandler() {
}

void StorageManagerHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "storageManagerPage",
                IDS_OPTIONS_SETTINGS_STORAGE_MANAGER_TAB_TITLE);

  localized_strings->SetString(
      "storageItemLabelCapacity", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_ITEM_LABEL_CAPACITY));
  localized_strings->SetString(
      "storageItemLabelInUse", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_ITEM_LABEL_IN_USE));
  localized_strings->SetString(
      "storageItemLabelAvailable", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_ITEM_LABEL_AVAILABLE));
  localized_strings->SetString(
      "storageSubitemLabelDownloads", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SUBITEM_LABEL_DOWNLOADS));
  localized_strings->SetString(
      "storageSubitemLabelArc", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SUBITEM_LABEL_ARC));
  localized_strings->SetString(
      "storageSizeCalculating", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SIZE_CALCULATING));
}

void StorageManagerHandler::InitializePage() {
  DCHECK(web_ui());
}

void StorageManagerHandler::RegisterMessages() {
  DCHECK(web_ui());

  web_ui()->RegisterMessageCallback(
      "updateStorageInfo",
      base::Bind(&StorageManagerHandler::HandleUpdateStorageInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openDownloads",
      base::Bind(&StorageManagerHandler::HandleOpenDownloads,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openArcStorage",
      base::Bind(&StorageManagerHandler::HandleOpenArcStorage,
                 base::Unretained(this)));
}

void StorageManagerHandler::HandleUpdateStorageInfo(
    const base::ListValue* unused_args) {
  UpdateSizeStat();
  UpdateDownloadsSize();
  UpdateArcSize();
}

void StorageManagerHandler::HandleOpenDownloads(
    const base::ListValue* unused_args) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  platform_util::OpenItem(
       profile,
       downloads_path,
       platform_util::OPEN_FOLDER,
       platform_util::OpenOperationCallback());
}

void StorageManagerHandler::HandleOpenArcStorage(
    const base::ListValue* unused_args) {
  arc::ArcStorageManager::Get()->OpenPrivateVolumeSettings();
}

void StorageManagerHandler::UpdateSizeStat() {
  Profile* const profile = Profile::FromWebUI(web_ui());
  const base::FilePath downloads_path =
     file_manager::util::GetDownloadsFolderForProfile(profile);

  int64_t* total_size = new int64_t(0);
  int64_t* available_size = new int64_t(0);
  content::BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&GetSizeStatOnBlockingPool,
                 downloads_path,
                 total_size,
                 available_size),
      base::Bind(&StorageManagerHandler::OnGetSizeStat,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(total_size),
                 base::Owned(available_size)));
}

void StorageManagerHandler::OnGetSizeStat(int64_t* total_size,
                                          int64_t* available_size) {
  int64_t used_size = *total_size - *available_size;
  base::DictionaryValue size_stat;
  size_stat.SetString("totalSize", ui::FormatBytes(*total_size));
  size_stat.SetString("availableSize", ui::FormatBytes(*available_size));
  size_stat.SetString("usedSize", ui::FormatBytes(used_size));
  size_stat.SetDouble("usedRatio",
                      static_cast<double>(used_size) / *total_size);
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.StorageManager.setSizeStat", size_stat);
}

void StorageManagerHandler::UpdateDownloadsSize() {
  Profile* const profile = Profile::FromWebUI(web_ui());
  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile);

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&base::ComputeDirectorySize, downloads_path),
      base::Bind(&StorageManagerHandler::OnGetDownloadsSize,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageManagerHandler::OnGetDownloadsSize(int64_t size) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.StorageManager.setDownloadsSize",
      base::StringValue(ui::FormatBytes(size)));
}

void StorageManagerHandler::UpdateArcSize() {
  Profile* const profile = Profile::FromWebUI(web_ui());
  if (!arc::ArcAuthService::IsAllowedForProfile(profile) ||
      arc::ArcAuthService::IsOptInVerificationDisabled() ||
      !arc::ArcAuthService::Get()->IsArcEnabled()) {
    return;
  }

  // Shows the item "Android apps and cache" and start calculating size.
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.StorageManager.showArcItem");
  bool success = arc::ArcStorageManager::Get()->GetApplicationsSize(
      base::Bind(&StorageManagerHandler::OnGetArcSize,
                 base::Unretained(this)));
  if (!success) {
    // TODO(fukino): Retry updating arc size later if the arc bridge service is
    // not ready.
    web_ui()->CallJavascriptFunctionUnsafe(
        "options.StorageManager.setArcSize",
        base::StringValue(l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_STORAGE_SIZE_UNKNOWN)));
  }
}

void StorageManagerHandler::OnGetArcSize(bool succeeded,
                                         arc::mojom::ApplicationsSizePtr size) {
  base::StringValue arc_size(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_STORAGE_SIZE_UNKNOWN));
  if (succeeded) {
    uint64_t total_bytes = size->total_code_bytes +
                           size->total_data_bytes +
                           size->total_cache_bytes;
    arc_size = base::StringValue(ui::FormatBytes(total_bytes));
  }
  web_ui()->CallJavascriptFunctionUnsafe("options.StorageManager.setArcSize",
                                         arc_size);
}

}  // namespace options
}  // namespace chromeos
