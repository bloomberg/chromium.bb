// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/storage_manager_handler.h"

#include <algorithm>
#include <numeric>
#include <string>

#include "base/files/file_util.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cache_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "components/arc/arc_util.h"
#include "components/browsing_data/content/conditional_cache_counting_helper.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace chromeos {
namespace options {
namespace {

void GetSizeStatAsync(const base::FilePath& mount_path,
                      int64_t* total_size,
                      int64_t* available_size) {
  int64_t size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  if (size >= 0)
    *total_size = size;
  size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  if (size >= 0)
    *available_size = size;
}

// Threshold to show a message indicating space is critically low (512 MB).
const int64_t kSpaceCriticallyLowBytes = 512 * 1024 * 1024;

// Threshold to show a message indicating space is low (1 GB).
const int64_t kSpaceLowBytes = 1 * 1024 * 1024 * 1024;

}  // namespace

StorageManagerHandler::StorageManagerHandler()
    : browser_cache_size_(-1),
      has_browser_cache_size_(false),
      browser_site_data_size_(-1),
      has_browser_site_data_size_(false),
      updating_downloads_size_(false),
      updating_drive_cache_size_(false),
      updating_browsing_data_size_(false),
      updating_arc_size_(false),
      updating_other_users_size_(false),
      weak_ptr_factory_(this) {
}

StorageManagerHandler::~StorageManagerHandler() {
}

void StorageManagerHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "storageManagerPage",
                IDS_OPTIONS_SETTINGS_STORAGE_MANAGER_TAB_TITLE);
  RegisterTitle(localized_strings, "storageClearDriveCachePage",
                IDS_OPTIONS_SETTINGS_STORAGE_CLEAR_DRIVE_CACHE_TAB_TITLE);

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
      "storageSubitemLabelDriveCache", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SUBITEM_LABEL_DRIVE_CACHE));
  localized_strings->SetString(
      "storageSubitemLabelBrowsingData", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SUBITEM_LABEL_BROWSING_DATA));
  localized_strings->SetString(
      "storageSubitemLabelOtherUsers", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SUBITEM_LABEL_OTHER_USERS));
  localized_strings->SetString(
      "storageSubitemLabelArc", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SUBITEM_LABEL_ARC));
  localized_strings->SetString(
      "storageSizeCalculating", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SIZE_CALCULATING));
  localized_strings->SetString(
      "storageClearDriveCacheDialogTitle", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_CLEAR_DRIVE_CACHE_DIALOG_TITLE));
  localized_strings->SetString(
      "storageClearDriveCacheDescription", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_CLEAR_DRIVE_CACHE_DESCRIPTION));
  localized_strings->SetString(
      "storageDeleteAllButtonTitle", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_DELETE_ALL_BUTTON_TITLE));
  localized_strings->SetString(
      "storageLowMessageTitle", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_TITLE));
  localized_strings->SetString(
      "storageLowMessageLine1", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_1));
  localized_strings->SetString(
      "storageLowMessageLine2", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_2));
  localized_strings->SetString(
      "storageCriticallyLowMessageTitle", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_TITLE));
  localized_strings->SetString(
      "storageCriticallyLowMessageLine1", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_1));
  localized_strings->SetString(
      "storageCriticallyLowMessageLine2", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_2));
}

void StorageManagerHandler::InitializePage() {
  DCHECK(web_ui());
}

void StorageManagerHandler::RegisterMessages() {
  DCHECK(web_ui());

  web_ui()->RegisterMessageCallback(
      "updateStorageInfo",
      base::Bind(&StorageManagerHandler::HandleUpdateStorageInfo,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "openDownloads",
      base::Bind(&StorageManagerHandler::HandleOpenDownloads,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "openArcStorage",
      base::Bind(&StorageManagerHandler::HandleOpenArcStorage,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "clearDriveCache",
      base::Bind(&StorageManagerHandler::HandleClearDriveCache,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageManagerHandler::HandleUpdateStorageInfo(
    const base::ListValue* unused_args) {
  UpdateSizeStat();
  UpdateDownloadsSize();
  UpdateDriveCacheSize();
  UpdateBrowsingDataSize();
  UpdateOtherUsersSize();
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

void StorageManagerHandler::HandleClearDriveCache(
    const base::ListValue* unused_args) {
  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(Profile::FromWebUI(web_ui()));
  file_system->FreeDiskSpaceIfNeededFor(
      std::numeric_limits<int64_t>::max(),  // Removes as much as possible.
      base::Bind(&StorageManagerHandler::OnClearDriveCacheDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageManagerHandler::UpdateSizeStat() {
  Profile* const profile = Profile::FromWebUI(web_ui());
  const base::FilePath downloads_path =
     file_manager::util::GetDownloadsFolderForProfile(profile);

  int64_t* total_size = new int64_t(0);
  int64_t* available_size = new int64_t(0);
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&GetSizeStatAsync, downloads_path, total_size, available_size),
      base::Bind(&StorageManagerHandler::OnGetSizeStat,
                 weak_ptr_factory_.GetWeakPtr(), base::Owned(total_size),
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
  int storage_space_state = STORAGE_SPACE_NORMAL;
  if (*available_size < kSpaceCriticallyLowBytes)
    storage_space_state = STORAGE_SPACE_CRITICALLY_LOW;
  else if (*available_size < kSpaceLowBytes)
    storage_space_state = STORAGE_SPACE_LOW;
  size_stat.SetInteger("spaceState", storage_space_state);

  web_ui()->CallJavascriptFunctionUnsafe(
      "options.StorageManager.setSizeStat", size_stat);
}

void StorageManagerHandler::UpdateDownloadsSize() {
  if (updating_downloads_size_)
    return;
  updating_downloads_size_ = true;

  Profile* const profile = Profile::FromWebUI(web_ui());
  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&base::ComputeDirectorySize, downloads_path),
      base::Bind(&StorageManagerHandler::OnGetDownloadsSize,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageManagerHandler::OnGetDownloadsSize(int64_t size) {
  updating_downloads_size_ = false;
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.StorageManager.setDownloadsSize",
      base::StringValue(ui::FormatBytes(size)));
}

void StorageManagerHandler::UpdateDriveCacheSize() {
  if (updating_drive_cache_size_)
    return;

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(Profile::FromWebUI(web_ui()));
  if (!file_system)
    return;

  // Shows the item "Offline cache" and start calculating size.
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.StorageManager.showDriveCacheItem");
  updating_drive_cache_size_ = true;
  file_system->CalculateCacheSize(
      base::Bind(&StorageManagerHandler::OnGetDriveCacheSize,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageManagerHandler::OnGetDriveCacheSize(int64_t size) {
  updating_drive_cache_size_ = false;
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.StorageManager.setDriveCacheSize",
      base::StringValue(ui::FormatBytes(size)));
}

void StorageManagerHandler::UpdateBrowsingDataSize() {
  if (updating_browsing_data_size_)
    return;
  updating_browsing_data_size_ = true;

  has_browser_cache_size_ = false;
  has_browser_site_data_size_ = false;
  Profile* const profile = Profile::FromWebUI(web_ui());
  // Fetch the size of http cache in browsing data.
  // ConditionalCacheCountingHelper deletes itself when it is done.
  browsing_data::ConditionalCacheCountingHelper::CreateForRange(
      content::BrowserContext::GetDefaultStoragePartition(profile),
      base::Time(), base::Time::Max())
      ->CountAndDestroySelfWhenFinished(
          base::Bind(&StorageManagerHandler::OnGetCacheSize,
                     weak_ptr_factory_.GetWeakPtr()));

  // Fetch the size of site data in browsing data.
  if (!site_data_size_collector_.get()) {
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile);
    site_data_size_collector_.reset(new SiteDataSizeCollector(
        storage_partition->GetPath(),
        new BrowsingDataCookieHelper(profile->GetRequestContext()),
        new BrowsingDataDatabaseHelper(profile),
        new BrowsingDataLocalStorageHelper(profile),
        new BrowsingDataAppCacheHelper(profile),
        new BrowsingDataIndexedDBHelper(
            storage_partition->GetIndexedDBContext()),
        BrowsingDataFileSystemHelper::Create(
            storage_partition->GetFileSystemContext()),
        BrowsingDataChannelIDHelper::Create(profile->GetRequestContext()),
        new BrowsingDataServiceWorkerHelper(
            storage_partition->GetServiceWorkerContext()),
        new BrowsingDataCacheStorageHelper(
            storage_partition->GetCacheStorageContext()),
        BrowsingDataFlashLSOHelper::Create(profile)));
  }
  site_data_size_collector_->Fetch(
      base::Bind(&StorageManagerHandler::OnGetBrowsingDataSize,
                 weak_ptr_factory_.GetWeakPtr(), true));
}

void StorageManagerHandler::OnGetCacheSize(int64_t size, bool is_upper_limit) {
  DCHECK(!is_upper_limit);
  OnGetBrowsingDataSize(false, size);
}

void StorageManagerHandler::OnGetBrowsingDataSize(bool is_site_data,
                                                  int64_t size) {
  if (is_site_data) {
    has_browser_site_data_size_ = true;
    browser_site_data_size_ = size;
  } else {
    has_browser_cache_size_ = true;
    browser_cache_size_ = size;
  }
  if (has_browser_cache_size_ && has_browser_site_data_size_) {
    base::string16 size_string;
    if (browser_cache_size_ >= 0 && browser_site_data_size_ >= 0) {
      size_string = ui::FormatBytes(
          browser_site_data_size_ + browser_cache_size_);
    } else {
      size_string = l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SIZE_UNKNOWN);
    }
    updating_browsing_data_size_ = false;
    web_ui()->CallJavascriptFunctionUnsafe(
        "options.StorageManager.setBrowsingDataSize",
        base::StringValue(size_string));
  }
}

void StorageManagerHandler::UpdateOtherUsersSize() {
  if (updating_other_users_size_)
    return;
  updating_other_users_size_ = true;

  other_users_.clear();
  user_sizes_.clear();
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  for (auto* user : users) {
    if (user->is_active())
      continue;
    other_users_.push_back(user);
    cryptohome::HomedirMethods::GetInstance()->GetAccountDiskUsage(
        cryptohome::Identification(user->GetAccountId()),
        base::Bind(&StorageManagerHandler::OnGetOtherUserSize,
                   weak_ptr_factory_.GetWeakPtr()));
  }
  // We should show "0 B" if there is no other user.
  if (other_users_.empty()) {
    updating_other_users_size_ = false;
    web_ui()->CallJavascriptFunctionUnsafe(
        "options.StorageManager.setOtherUsersSize",
        base::StringValue(ui::FormatBytes(0)));
  }
}

void StorageManagerHandler::OnGetOtherUserSize(bool success, int64_t size) {
  user_sizes_.push_back(success ? size : -1);
  if (user_sizes_.size() == other_users_.size()) {
    base::string16 size_string;
    // If all the requests succeed, shows the total bytes in the UI.
    if (std::count(user_sizes_.begin(), user_sizes_.end(), -1) == 0) {
      size_string = ui::FormatBytes(
          std::accumulate(user_sizes_.begin(), user_sizes_.end(), 0LL));
    } else {
      size_string = l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_STORAGE_SIZE_UNKNOWN);
    }
    updating_other_users_size_ = false;
    web_ui()->CallJavascriptFunctionUnsafe(
        "options.StorageManager.setOtherUsersSize",
        base::StringValue(size_string));
  }
}

void StorageManagerHandler::UpdateArcSize() {
  if (updating_arc_size_)
    return;
  updating_arc_size_ = true;

  Profile* const profile = Profile::FromWebUI(web_ui());
  if (!arc::IsArcPlayStoreEnabledForProfile(profile) ||
      arc::IsArcOptInVerificationDisabled()) {
    return;
  }

  // Shows the item "Android apps and cache" and start calculating size.
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.StorageManager.showArcItem");
  bool success = arc::ArcStorageManager::Get()->GetApplicationsSize(
      base::Bind(&StorageManagerHandler::OnGetArcSize,
                 weak_ptr_factory_.GetWeakPtr()));
  if (!success)
    updating_arc_size_ = false;
}

void StorageManagerHandler::OnGetArcSize(bool succeeded,
                                         arc::mojom::ApplicationsSizePtr size) {
  base::string16 size_string;
  if (succeeded) {
    uint64_t total_bytes = size->total_code_bytes +
                           size->total_data_bytes +
                           size->total_cache_bytes;
    size_string = ui::FormatBytes(total_bytes);
  } else {
    size_string = l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_STORAGE_SIZE_UNKNOWN);
  }
  updating_arc_size_ = false;
  web_ui()->CallJavascriptFunctionUnsafe("options.StorageManager.setArcSize",
                                         base::StringValue(size_string));
}

void StorageManagerHandler::OnClearDriveCacheDone(bool success) {
  UpdateDriveCacheSize();
}

}  // namespace options
}  // namespace chromeos
