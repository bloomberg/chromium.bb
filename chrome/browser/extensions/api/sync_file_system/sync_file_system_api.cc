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
using sync_file_system::SyncFileStatus;
using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncStatusCode;

namespace extensions {

namespace {

// This is the only supported cloud backend service for now.
const char* const kDriveCloudService =
    sync_file_system::DriveFileSyncService::kServiceName;

// Error messages.
const char kFileError[] = "File error %d.";
const char kQuotaError[] = "Quota error %d.";

api::sync_file_system::FileStatus FileSyncStatusEnumToExtensionEnum(
    const SyncFileStatus state) {
  switch (state) {
    case sync_file_system::SYNC_FILE_STATUS_UNKNOWN:
      return api::sync_file_system::FILE_STATUS_NONE;
    case sync_file_system::SYNC_FILE_STATUS_SYNCED:
      return api::sync_file_system::FILE_STATUS_SYNCED;
    case sync_file_system::SYNC_FILE_STATUS_HAS_PENDING_CHANGES:
      return api::sync_file_system::FILE_STATUS_PENDING;
    case sync_file_system::SYNC_FILE_STATUS_CONFLICTING:
      return api::sync_file_system::FILE_STATUS_CONFLICTING;
  }
  NOTREACHED();
  return api::sync_file_system::FILE_STATUS_NONE;
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

}  // namespace

bool SyncFileSystemDeleteFileSystemFunction::RunImpl() {
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url));

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(
          profile(),
          render_view_host()->GetSiteInstance())->GetFileSystemContext();
  fileapi::FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(url)));

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
  // Please note that Google Drive is the only supported cloud backend at this
  // time. However other functions which have already been written to
  // accommodate different service names are being left as is to allow easier
  // future support for other backend services. (http://crbug.com/172562).
  const std::string service_name = sync_file_system::DriveFileSyncService::
      kServiceName;
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
    SyncStatusCode status) {
  if (status != sync_file_system::SYNC_STATUS_OK) {
    error_ = sync_file_system::SyncStatusCodeToString(status);
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

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(
          profile(),
          render_view_host()->GetSiteInstance())->GetFileSystemContext();
  fileapi::FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(url)));

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

bool SyncFileSystemGetFileStatusFunction::RunImpl() {
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url));

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(
          profile(),
          render_view_host()->GetSiteInstance())->GetFileSystemContext();
  fileapi::FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(url)));

  SyncFileSystemServiceFactory::GetForProfile(profile())->GetFileSyncStatus(
      file_system_url,
      Bind(&SyncFileSystemGetFileStatusFunction::DidGetFileStatus,
           this));
  return true;
}

void SyncFileSystemGetFileStatusFunction::DidGetFileStatus(
    const SyncStatusCode sync_service_status,
    const SyncFileStatus sync_file_status) {
  // Repost to switch from IO thread to UI thread for SendResponse().
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        Bind(&SyncFileSystemGetFileStatusFunction::DidGetFileStatus,
             this, sync_service_status, sync_file_status));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (sync_service_status != sync_file_system::SYNC_STATUS_OK) {
    error_ = sync_file_system::SyncStatusCodeToString(sync_service_status);
    SendResponse(false);
    return;
  }

  // Convert from C++ to JavaScript enum.
  results_ = api::sync_file_system::GetFileStatus::Results::Create(
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
