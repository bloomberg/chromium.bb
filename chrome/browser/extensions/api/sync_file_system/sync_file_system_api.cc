// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sync_file_system/sync_file_system_api.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer.h"
#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/common/extensions/api/sync_file_system.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/syncable/sync_file_status.h"
#include "webkit/quota/quota_manager.h"

using content::BrowserContext;
using content::BrowserThread;
using sync_file_system::SyncFileSystemServiceFactory;

namespace extensions {

namespace {

// This is the only supported cloud backend service for now.
const char* const kDriveCloudService =
    sync_file_system::DriveFileSyncService::kServiceName;

// Error messages.
const char kNotSupportedService[] = "Cloud service %s not supported.";
const char kFileError[] = "File error %d.";
const char kQuotaError[] = "Quota error %d.";

api::sync_file_system::FileSyncStatus FileSyncStatusEnumToExtensionEnum(
    const fileapi::SyncFileStatus state) {
  switch (state) {
    case fileapi::SYNC_FILE_STATUS_UNKNOWN:
      return api::sync_file_system::SYNC_FILE_SYSTEM_FILE_SYNC_STATUS_NONE;
    case fileapi::SYNC_FILE_STATUS_SYNCED:
      return api::sync_file_system::SYNC_FILE_SYSTEM_FILE_SYNC_STATUS_SYNCED;
    case fileapi::SYNC_FILE_STATUS_HAS_PENDING_CHANGES:
      return api::sync_file_system::SYNC_FILE_SYSTEM_FILE_SYNC_STATUS_PENDING;
    case fileapi::SYNC_FILE_STATUS_CONFLICTING:
      return api::sync_file_system::
          SYNC_FILE_SYSTEM_FILE_SYNC_STATUS_CONFLICTING;
  }
  NOTREACHED();
  return api::sync_file_system::SYNC_FILE_SYSTEM_FILE_SYNC_STATUS_NONE;
}

sync_file_system::SyncFileSystemService* GetSyncFileSystemService(
    Profile* profile) {
  sync_file_system::SyncFileSystemService* service =
      SyncFileSystemServiceFactory::GetForProfile(profile);
  DCHECK(service);
  ExtensionSyncEventObserver* observer =
      ExtensionSyncEventObserverFactory::GetForProfile(profile);
  DCHECK(observer);
  observer->InitializeForService(service, kDriveCloudService);
  return service;
}

bool IsValidServiceName(const std::string& service_name, std::string* error) {
  DCHECK(error);
  if (service_name != std::string(kDriveCloudService)) {
    *error = base::StringPrintf(kNotSupportedService, service_name.c_str());
    return false;
  }
  return true;
}

}  // namespace

bool SyncFileSystemDeleteFileSystemFunction::RunImpl() {
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url));
  fileapi::FileSystemURL file_system_url((GURL(url)));
  if (!IsValidServiceName(file_system_url.filesystem_id(), &error_)) {
    return false;
  }

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(
          profile(),
          render_view_host()->GetSiteInstance())->GetFileSystemContext();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      Bind(&fileapi::FileSystemContext::DeleteFileSystem,
           file_system_context,
           source_url().GetOrigin(),
           file_system_url.type(),
           Bind(&SyncFileSystemDeleteFileSystemFunction::DidDeleteFileSystem,
                this)));
  return true;
}

void SyncFileSystemDeleteFileSystemFunction::DidDeleteFileSystem(
    base::PlatformFileError error) {
  // Repost to switch from IO thread to UI thread for SendResponse().
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        Bind(&SyncFileSystemDeleteFileSystemFunction::DidDeleteFileSystem, this,
             error));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error != base::PLATFORM_FILE_OK) {
    error_ = base::StringPrintf(kFileError, static_cast<int>(error));
    SetResult(base::Value::CreateBooleanValue(false));
    SendResponse(false);
    return;
  }

  SetResult(base::Value::CreateBooleanValue(true));
  SendResponse(true);
}

bool SyncFileSystemRequestFileSystemFunction::RunImpl() {
  std::string service_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &service_name));
  if (!IsValidServiceName(service_name, &error_)) {
    return false;
  }

  // Initializes sync context for this extension and continue to open
  // a new file system.
  GetSyncFileSystemService(profile())->
      InitializeForApp(
          GetFileSystemContext(),
          service_name,
          source_url().GetOrigin(),
          base::Bind(&self::DidInitializeFileSystemContext, this,
                     service_name));
  return true;
}

fileapi::FileSystemContext*
SyncFileSystemRequestFileSystemFunction::GetFileSystemContext() {
  return BrowserContext::GetStoragePartition(
      profile(),
      render_view_host()->GetSiteInstance())->GetFileSystemContext();
}

void SyncFileSystemRequestFileSystemFunction::DidInitializeFileSystemContext(
    const std::string& service_name,
    fileapi::SyncStatusCode status) {
  if (status != fileapi::SYNC_STATUS_OK) {
    error_ = fileapi::SyncStatusCodeToString(status);
    SendResponse(false);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&fileapi::FileSystemContext::OpenSyncableFileSystem,
           GetFileSystemContext(),
           service_name,
           source_url().GetOrigin(),
           fileapi::kFileSystemTypeSyncable,
           true, /* create */
           base::Bind(&self::DidOpenFileSystem, this)));
}

void SyncFileSystemRequestFileSystemFunction::DidOpenFileSystem(
    base::PlatformFileError error,
    const std::string& file_system_name,
    const GURL& root_url) {
  // Repost to switch from IO thread to UI thread for SendResponse().
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        Bind(&SyncFileSystemRequestFileSystemFunction::DidOpenFileSystem,
             this, error, file_system_name, root_url));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error != base::PLATFORM_FILE_OK) {
    error_ = base::StringPrintf(kFileError, static_cast<int>(error));
    SendResponse(false);
    return;
  }

  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);
  dict->SetString("name", file_system_name);
  dict->SetString("root", root_url.spec());
  SendResponse(true);
}

bool SyncFileSystemGetUsageAndQuotaFunction::RunImpl() {
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url));
  fileapi::FileSystemURL file_system_url((GURL(url)));
  if (!IsValidServiceName(file_system_url.filesystem_id(), &error_)) {
    return false;
  }

  scoped_refptr<quota::QuotaManager> quota_manager =
      BrowserContext::GetStoragePartition(
          profile(),
          render_view_host()->GetSiteInstance())->GetQuotaManager();

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      Bind(&quota::QuotaManager::GetUsageAndQuota,
           quota_manager,
           source_url().GetOrigin(),
           fileapi::FileSystemTypeToQuotaStorageType(file_system_url.type()),
           Bind(&SyncFileSystemGetUsageAndQuotaFunction::DidGetUsageAndQuota,
                this)));

  return true;
}

bool SyncFileSystemGetFileSyncStatusFunction::RunImpl() {
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url));
  fileapi::FileSystemURL file_system_url((GURL(url)));

  SyncFileSystemServiceFactory::GetForProfile(profile())->GetFileSyncStatus(
      file_system_url,
      Bind(&SyncFileSystemGetFileSyncStatusFunction::DidGetFileSyncStatus,
           this));
  return true;
}

void SyncFileSystemGetFileSyncStatusFunction::DidGetFileSyncStatus(
    const fileapi::SyncStatusCode sync_service_status,
    const fileapi::SyncFileStatus sync_file_status) {
  // Repost to switch from IO thread to UI thread for SendResponse().
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        Bind(&SyncFileSystemGetFileSyncStatusFunction::DidGetFileSyncStatus,
             this, sync_service_status, sync_file_status));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (sync_service_status != fileapi::SYNC_STATUS_OK) {
    error_ = fileapi::SyncStatusCodeToString(sync_service_status);
    SendResponse(false);
    return;
  }

  // Convert from C++ to JavaScript enum.
  results_ = api::sync_file_system::GetFileSyncStatus::Results::Create(
      FileSyncStatusEnumToExtensionEnum(sync_file_status));
  SendResponse(true);
}

void SyncFileSystemGetUsageAndQuotaFunction::DidGetUsageAndQuota(
      quota::QuotaStatusCode status, int64 usage, int64 quota) {
  // Repost to switch from IO thread to UI thread for SendResponse().
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        Bind(&SyncFileSystemGetUsageAndQuotaFunction::DidGetUsageAndQuota, this,
             status, usage, quota));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (status != quota::kQuotaStatusOk) {
    error_ = QuotaStatusCodeToString(status);
    SendResponse(false);
    return;
  }

  api::sync_file_system::StorageInfo info;
  info.usage_bytes = usage;
  info.quota_bytes = quota;
  results_ = api::sync_file_system::GetUsageAndQuota::Results::Create(info);
  SendResponse(true);
}

}  // namespace extensions
