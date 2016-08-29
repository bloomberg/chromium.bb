// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_external_updater.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/ui/kiosk_external_update_notification.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

const char kExternalUpdateManifest[] = "external_update.json";
const char kExternalCrx[] = "external_crx";
const char kExternalVersion[] = "external_version";

void ParseExternalUpdateManifest(
    const base::FilePath& external_update_dir,
    base::DictionaryValue* parsed_manifest,
    KioskExternalUpdater::ExternalUpdateErrorCode* error_code) {
  base::FilePath manifest =
      external_update_dir.AppendASCII(kExternalUpdateManifest);
  if (!base::PathExists(manifest)) {
    *error_code = KioskExternalUpdater::ERROR_NO_MANIFEST;
    return;
  }

  JSONFileValueDeserializer deserializer(manifest);
  std::string error_msg;
  base::Value* extensions =
      deserializer.Deserialize(NULL, &error_msg).release();
  // TODO(Olli Raula) possible memory leak http://crbug.com/543015
  if (!extensions) {
    *error_code = KioskExternalUpdater::ERROR_INVALID_MANIFEST;
    return;
  }

  base::DictionaryValue* dict_value = NULL;
  if (!extensions->GetAsDictionary(&dict_value)) {
    *error_code = KioskExternalUpdater::ERROR_INVALID_MANIFEST;
    return;
  }

  parsed_manifest->Swap(dict_value);
  *error_code = KioskExternalUpdater::ERROR_NONE;
}

// Copies |external_crx_file| to |temp_crx_file|, and removes |temp_dir|
// created for unpacking |external_crx_file|.
void CopyExternalCrxAndDeleteTempDir(const base::FilePath& external_crx_file,
                                     const base::FilePath& temp_crx_file,
                                     const base::FilePath& temp_dir,
                                     bool* success) {
  base::DeleteFile(temp_dir, true);
  *success = base::CopyFile(external_crx_file, temp_crx_file);
}

// Returns true if |version_1| < |version_2|, and
// if |update_for_same_version| is true and |version_1| = |version_2|.
bool ShouldUpdateForHigherVersion(const std::string& version_1,
                                  const std::string& version_2,
                                  bool update_for_same_version) {
  const base::Version v1(version_1);
  const base::Version v2(version_2);
  if (!v1.IsValid() || !v2.IsValid())
    return false;
  int compare_result = v1.CompareTo(v2);
  if (compare_result < 0)
    return true;
  else if (update_for_same_version && compare_result == 0)
    return true;
  else
    return false;
}

}  // namespace

KioskExternalUpdater::ExternalUpdate::ExternalUpdate() {
}

KioskExternalUpdater::ExternalUpdate::ExternalUpdate(
    const ExternalUpdate& other) = default;

KioskExternalUpdater::ExternalUpdate::~ExternalUpdate() {
}

KioskExternalUpdater::KioskExternalUpdater(
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    const base::FilePath& crx_cache_dir,
    const base::FilePath& crx_unpack_dir)
    : backend_task_runner_(backend_task_runner),
      crx_cache_dir_(crx_cache_dir),
      crx_unpack_dir_(crx_unpack_dir),
      weak_factory_(this) {
  // Subscribe to DiskMountManager.
  DCHECK(disks::DiskMountManager::GetInstance());
  disks::DiskMountManager::GetInstance()->AddObserver(this);
}

KioskExternalUpdater::~KioskExternalUpdater() {
  if (disks::DiskMountManager::GetInstance())
    disks::DiskMountManager::GetInstance()->RemoveObserver(this);
}

void KioskExternalUpdater::OnDiskEvent(
    disks::DiskMountManager::DiskEvent event,
    const disks::DiskMountManager::Disk* disk) {
}

void KioskExternalUpdater::OnDeviceEvent(
    disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
}

void KioskExternalUpdater::OnMountEvent(
    disks::DiskMountManager::MountEvent event,
    MountError error_code,
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (mount_info.mount_type != MOUNT_TYPE_DEVICE ||
      error_code != MOUNT_ERROR_NONE) {
    return;
  }

  if (event == disks::DiskMountManager::MOUNTING) {
    // If multiple disks have been mounted, skip the rest of them if kiosk
    // update has already been found.
    if (!external_update_path_.empty()) {
      LOG(WARNING) << "External update path already found, skip "
                   << mount_info.mount_path;
      return;
    }

    base::DictionaryValue* parsed_manifest = new base::DictionaryValue();
    ExternalUpdateErrorCode* parsing_error = new ExternalUpdateErrorCode;
    backend_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&ParseExternalUpdateManifest,
                   base::FilePath(mount_info.mount_path),
                   parsed_manifest,
                   parsing_error),
        base::Bind(&KioskExternalUpdater::ProcessParsedManifest,
                   weak_factory_.GetWeakPtr(),
                   base::Owned(parsing_error),
                   base::FilePath(mount_info.mount_path),
                   base::Owned(parsed_manifest)));
  } else {  // unmounting a removable device.
    if (external_update_path_.value().empty()) {
      // Clear any previously displayed message.
      DismissKioskUpdateNotification();
    } else if (external_update_path_.value() == mount_info.mount_path) {
      DismissKioskUpdateNotification();
      if (IsExternalUpdatePending()) {
        LOG(ERROR) << "External kiosk update is not completed when the usb "
                      "stick is unmoutned.";
      }
      external_updates_.clear();
      external_update_path_.clear();
    }
  }
}

void KioskExternalUpdater::OnFormatEvent(
    disks::DiskMountManager::FormatEvent event,
    FormatError error_code,
    const std::string& device_path) {
}

void KioskExternalUpdater::OnExtenalUpdateUnpackSuccess(
    const std::string& app_id,
    const std::string& version,
    const std::string& min_browser_version,
    const base::FilePath& temp_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // User might pull out the usb stick before updating is completed.
  if (CheckExternalUpdateInterrupted())
    return;

  if (!ShouldDoExternalUpdate(app_id, version, min_browser_version)) {
    external_updates_[app_id].update_status = FAILED;
    MaybeValidateNextExternalUpdate();
    return;
  }

  // User might pull out the usb stick before updating is completed.
  if (CheckExternalUpdateInterrupted())
    return;

  base::FilePath external_crx_path =
      external_updates_[app_id].external_crx.path;
  base::FilePath temp_crx_path =
      crx_unpack_dir_.Append(external_crx_path.BaseName());
  bool* success = new bool;
  backend_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CopyExternalCrxAndDeleteTempDir,
                 external_crx_path,
                 temp_crx_path,
                 temp_dir,
                 success),
      base::Bind(&KioskExternalUpdater::PutValidatedExtension,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(success),
                 app_id,
                 temp_crx_path,
                 version));
}

void KioskExternalUpdater::OnExternalUpdateUnpackFailure(
    const std::string& app_id) {
  // User might pull out the usb stick before updating is completed.
  if (CheckExternalUpdateInterrupted())
    return;

  external_updates_[app_id].update_status = FAILED;
  external_updates_[app_id].error =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_KIOSK_EXTERNAL_UPDATE_BAD_CRX);
  MaybeValidateNextExternalUpdate();
}

void KioskExternalUpdater::ProcessParsedManifest(
    ExternalUpdateErrorCode* parsing_error,
    const base::FilePath& external_update_dir,
    base::DictionaryValue* parsed_manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (*parsing_error == ERROR_NO_MANIFEST) {
    KioskAppManager::Get()->OnKioskAppExternalUpdateComplete(false);
    return;
  } else if (*parsing_error == ERROR_INVALID_MANIFEST) {
    NotifyKioskUpdateProgress(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_KIOSK_EXTERNAL_UPDATE_INVALID_MANIFEST));
    KioskAppManager::Get()->OnKioskAppExternalUpdateComplete(false);
    return;
  }

  NotifyKioskUpdateProgress(
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_KIOSK_EXTERNAL_UPDATE_IN_PROGRESS));

  external_update_path_ = external_update_dir;
  for (base::DictionaryValue::Iterator it(*parsed_manifest); !it.IsAtEnd();
       it.Advance()) {
    std::string app_id = it.key();
    std::string cached_version_str;
    base::FilePath cached_crx;
    if (!KioskAppManager::Get()->GetCachedCrx(
            app_id, &cached_crx, &cached_version_str)) {
      LOG(WARNING) << "Can't find app in existing cache " << app_id;
      continue;
    }

    const base::DictionaryValue* extension = NULL;
    if (!it.value().GetAsDictionary(&extension)) {
      LOG(ERROR) << "Found bad entry in manifest type " << it.value().GetType();
      continue;
    }

    std::string external_crx_str;
    if (!extension->GetString(kExternalCrx, &external_crx_str)) {
      LOG(ERROR) << "Can't find external crx in manifest " << app_id;
      continue;
    }

    std::string external_version_str;
    if (extension->GetString(kExternalVersion, &external_version_str)) {
      if (!ShouldUpdateForHigherVersion(
              cached_version_str, external_version_str, false)) {
        LOG(WARNING) << "External app " << app_id
                     << " is at the same or lower version comparing to "
                     << " the existing one.";
        continue;
      }
    }

    ExternalUpdate update;
    KioskAppManager::App app;
    if (KioskAppManager::Get()->GetApp(app_id, &app)) {
      update.app_name = app.name;
    } else {
      NOTREACHED();
    }
    update.external_crx = extensions::CRXFileInfo(
        app_id, external_update_path_.AppendASCII(external_crx_str));
    update.update_status = PENDING;
    external_updates_[app_id] = update;
  }

  if (external_updates_.empty()) {
    NotifyKioskUpdateProgress(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_KIOSK_EXTERNAL_UPDATE_NO_UPDATES));
    KioskAppManager::Get()->OnKioskAppExternalUpdateComplete(false);
    return;
  }

  ValidateExternalUpdates();
}

bool KioskExternalUpdater::CheckExternalUpdateInterrupted() {
  if (external_updates_.empty()) {
    // This could happen if user pulls out the usb stick before the updating
    // operation is completed.
    LOG(ERROR) << "external_updates_ has been cleared before external "
               << "updating completes.";
    return true;
  }

  return false;
}

void KioskExternalUpdater::ValidateExternalUpdates() {
  for (ExternalUpdateMap::iterator it = external_updates_.begin();
       it != external_updates_.end();
       ++it) {
    if (it->second.update_status == PENDING) {
      scoped_refptr<KioskExternalUpdateValidator> crx_validator =
          new KioskExternalUpdateValidator(backend_task_runner_,
                                           it->second.external_crx,
                                           crx_unpack_dir_,
                                           weak_factory_.GetWeakPtr());
      crx_validator->Start();
      break;
    }
  }
}

bool KioskExternalUpdater::IsExternalUpdatePending() {
  for (ExternalUpdateMap::iterator it = external_updates_.begin();
       it != external_updates_.end();
       ++it) {
    if (it->second.update_status == PENDING) {
      return true;
    }
  }
  return false;
}

bool KioskExternalUpdater::IsAllExternalUpdatesSucceeded() {
  for (ExternalUpdateMap::iterator it = external_updates_.begin();
       it != external_updates_.end();
       ++it) {
    if (it->second.update_status != SUCCESS) {
      return false;
    }
  }
  return true;
}

bool KioskExternalUpdater::ShouldDoExternalUpdate(
    const std::string& app_id,
    const std::string& version,
    const std::string& min_browser_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string existing_version_str;
  base::FilePath existing_path;
  bool cached = KioskAppManager::Get()->GetCachedCrx(
      app_id, &existing_path, &existing_version_str);
  DCHECK(cached);

  // Compare app version.
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  if (!ShouldUpdateForHigherVersion(existing_version_str, version, false)) {
    external_updates_[app_id].error = rb->GetLocalizedString(
        IDS_KIOSK_EXTERNAL_UPDATE_SAME_OR_LOWER_APP_VERSION);
    return false;
  }

  // Check minimum browser version.
  if (!min_browser_version.empty()) {
    if (!ShouldUpdateForHigherVersion(min_browser_version,
                                      version_info::GetVersionNumber(), true)) {
      external_updates_[app_id].error = l10n_util::GetStringFUTF16(
          IDS_KIOSK_EXTERNAL_UPDATE_REQUIRE_HIGHER_BROWSER_VERSION,
          base::UTF8ToUTF16(min_browser_version));
      return false;
    }
  }

  return true;
}

void KioskExternalUpdater::PutValidatedExtension(bool* crx_copied,
                                                 const std::string& app_id,
                                                 const base::FilePath& crx_file,
                                                 const std::string& version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (CheckExternalUpdateInterrupted())
    return;

  if (!*crx_copied) {
    LOG(ERROR) << "Cannot copy external crx file to " << crx_file.value();
    external_updates_[app_id].update_status = FAILED;
    external_updates_[app_id].error = l10n_util::GetStringFUTF16(
        IDS_KIOSK_EXTERNAL_UPDATE_FAILED_COPY_CRX_TO_TEMP,
        base::UTF8ToUTF16(crx_file.value()));
    MaybeValidateNextExternalUpdate();
    return;
  }

  chromeos::KioskAppManager::Get()->PutValidatedExternalExtension(
      app_id,
      crx_file,
      version,
      base::Bind(&KioskExternalUpdater::OnPutValidatedExtension,
                 weak_factory_.GetWeakPtr()));
}

void KioskExternalUpdater::OnPutValidatedExtension(const std::string& app_id,
                                                   bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (CheckExternalUpdateInterrupted())
    return;

  if (!success) {
    external_updates_[app_id].update_status = FAILED;
    external_updates_[app_id].error = l10n_util::GetStringFUTF16(
        IDS_KIOSK_EXTERNAL_UPDATE_CANNOT_INSTALL_IN_LOCAL_CACHE,
        base::UTF8ToUTF16(external_updates_[app_id].external_crx.path.value()));
  } else {
    external_updates_[app_id].update_status = SUCCESS;
  }

  // Validate the next pending external update.
  MaybeValidateNextExternalUpdate();
}

void KioskExternalUpdater::MaybeValidateNextExternalUpdate() {
  if (IsExternalUpdatePending())
    ValidateExternalUpdates();
  else
    MayBeNotifyKioskAppUpdate();
}

void KioskExternalUpdater::MayBeNotifyKioskAppUpdate() {
  if (IsExternalUpdatePending())
    return;

  NotifyKioskUpdateProgress(GetUpdateReportMessage());
  NotifyKioskAppUpdateAvailable();
  KioskAppManager::Get()->OnKioskAppExternalUpdateComplete(
      IsAllExternalUpdatesSucceeded());
}

void KioskExternalUpdater::NotifyKioskAppUpdateAvailable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (ExternalUpdateMap::iterator it = external_updates_.begin();
       it != external_updates_.end();
       ++it) {
    if (it->second.update_status == SUCCESS) {
      KioskAppManager::Get()->OnKioskAppCacheUpdated(it->first);
    }
  }
}

void KioskExternalUpdater::NotifyKioskUpdateProgress(
    const base::string16& message) {
  if (!notification_)
    notification_.reset(new KioskExternalUpdateNotification(message));
  else
    notification_->ShowMessage(message);
}

void KioskExternalUpdater::DismissKioskUpdateNotification() {
  if (notification_.get()) {
    notification_.reset();
  }
}

base::string16 KioskExternalUpdater::GetUpdateReportMessage() {
  DCHECK(!IsExternalUpdatePending());
  int updated = 0;
  int failed = 0;
  base::string16 updated_apps;
  base::string16 failed_apps;
  for (ExternalUpdateMap::iterator it = external_updates_.begin();
       it != external_updates_.end();
       ++it) {
    base::string16 app_name = base::UTF8ToUTF16(it->second.app_name);
    if (it->second.update_status == SUCCESS) {
      ++updated;
      if (updated_apps.empty())
        updated_apps = app_name;
      else
        updated_apps = updated_apps + base::ASCIIToUTF16(", ") + app_name;
    } else {  // FAILED
      ++failed;
      if (failed_apps.empty()) {
        failed_apps = app_name + base::ASCIIToUTF16(": ") + it->second.error;
      } else {
        failed_apps = failed_apps + base::ASCIIToUTF16("\n") + app_name +
                      base::ASCIIToUTF16(": ") + it->second.error;
      }
    }
  }

  base::string16 message;
  message = ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
      IDS_KIOSK_EXTERNAL_UPDATE_COMPLETE);
  base::string16 success_app_msg;
  if (updated) {
    success_app_msg = l10n_util::GetStringFUTF16(
        IDS_KIOSK_EXTERNAL_UPDATE_SUCCESSFUL_UPDATED_APPS, updated_apps);
    message = message + base::ASCIIToUTF16("\n") + success_app_msg;
  }

  base::string16 failed_app_msg;
  if (failed) {
    failed_app_msg = ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                         IDS_KIOSK_EXTERNAL_UPDATE_FAILED_UPDATED_APPS) +
                     base::ASCIIToUTF16("\n") + failed_apps;
    message = message + base::ASCIIToUTF16("\n") + failed_app_msg;
  }
  return message;
}

}  // namespace chromeos
