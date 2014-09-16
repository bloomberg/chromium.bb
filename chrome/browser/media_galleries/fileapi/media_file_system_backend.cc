// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_validator_factory.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/extension_system.h"
#include "net/url_request/url_request.h"
#include "storage/browser/blob/file_stream_reader.h"
#include "storage/browser/fileapi/copy_or_move_file_validator.h"
#include "storage/browser/fileapi/file_stream_writer.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/native_file_util.h"
#include "storage/common/fileapi/file_system_types.h"
#include "storage/common/fileapi/file_system_util.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/media_galleries/fileapi/itunes_file_util.h"
#include "chrome/browser/media_galleries/fileapi/picasa_file_util.h"
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_MACOSX)
#include "chrome/browser/media_galleries/fileapi/iphoto_file_util.h"
#endif  // defined(OS_MACOSX)

using storage::FileSystemContext;
using storage::FileSystemURL;

namespace {

const char kMediaGalleryMountPrefix[] = "media_galleries-";

void OnPreferencesInit(
    const content::RenderViewHost* rvh,
    const extensions::Extension* extension,
    MediaGalleryPrefId pref_id,
    const base::Callback<void(base::File::Error result)>& callback) {
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  registry->RegisterMediaFileSystemForExtension(rvh, extension, pref_id,
                                                callback);
}

void AttemptAutoMountOnUIThread(
    int32 process_id,
    int32 routing_id,
    const std::string& storage_domain,
    const std::string& mount_point,
    const base::Callback<void(base::File::Error result)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, routing_id);
  if (rvh) {
    Profile* profile =
        Profile::FromBrowserContext(rvh->GetProcess()->GetBrowserContext());

    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    const extensions::Extension* extension =
        extension_service->GetExtensionById(storage_domain,
                                            false /*include disabled*/);
    std::string expected_mount_prefix =
        MediaFileSystemBackend::ConstructMountName(
            profile->GetPath(), storage_domain, kInvalidMediaGalleryPrefId);
    MediaGalleryPrefId pref_id = kInvalidMediaGalleryPrefId;
    if (extension &&
        extension->id() == storage_domain &&
        StartsWithASCII(mount_point, expected_mount_prefix, true) &&
        base::StringToUint64(mount_point.substr(expected_mount_prefix.size()),
                             &pref_id) &&
        pref_id != kInvalidMediaGalleryPrefId) {
      MediaGalleriesPreferences* preferences =
          g_browser_process->media_file_system_registry()->GetPreferences(
              profile);
      preferences->EnsureInitialized(
          base::Bind(&OnPreferencesInit, rvh, extension, pref_id, callback));
      return;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, base::File::FILE_ERROR_NOT_FOUND));
}

}  // namespace

const char MediaFileSystemBackend::kMediaTaskRunnerName[] =
    "media-task-runner";

MediaFileSystemBackend::MediaFileSystemBackend(
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* media_task_runner)
    : profile_path_(profile_path),
      media_task_runner_(media_task_runner),
      media_path_filter_(new MediaPathFilter),
      media_copy_or_move_file_validator_factory_(new MediaFileValidatorFactory),
      native_media_file_util_(
          new NativeMediaFileUtil(media_path_filter_.get())),
      device_media_async_file_util_(
          DeviceMediaAsyncFileUtil::Create(profile_path_,
                                           APPLY_MEDIA_FILE_VALIDATION))
#if defined(OS_WIN) || defined(OS_MACOSX)
      ,
      picasa_file_util_(new picasa::PicasaFileUtil(media_path_filter_.get())),
      itunes_file_util_(new itunes::ITunesFileUtil(media_path_filter_.get()))
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
#if defined(OS_MACOSX)
      ,
      iphoto_file_util_(new iphoto::IPhotoFileUtil(media_path_filter_.get()))
#endif  // defined(OS_MACOSX)
{
}

MediaFileSystemBackend::~MediaFileSystemBackend() {
}

// static
bool MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(kMediaTaskRunnerName);
  return pool->IsRunningSequenceOnCurrentThread(media_sequence_token);
}

// static
scoped_refptr<base::SequencedTaskRunner>
MediaFileSystemBackend::MediaTaskRunner() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(kMediaTaskRunnerName);
  return pool->GetSequencedTaskRunner(media_sequence_token);
}

// static
std::string MediaFileSystemBackend::ConstructMountName(
    const base::FilePath& profile_path,
    const std::string& extension_id,
    MediaGalleryPrefId pref_id) {
  std::string name(kMediaGalleryMountPrefix);
  name.append(profile_path.BaseName().MaybeAsASCII());
  name.append("-");
  name.append(extension_id);
  name.append("-");
  if (pref_id != kInvalidMediaGalleryPrefId)
    name.append(base::Uint64ToString(pref_id));
  base::ReplaceChars(name, " /", "_", &name);
  return name;
}

// static
bool MediaFileSystemBackend::AttemptAutoMountForURLRequest(
    const net::URLRequest* url_request,
    const storage::FileSystemURL& filesystem_url,
    const std::string& storage_domain,
    const base::Callback<void(base::File::Error result)>& callback) {
  if (storage_domain.empty() ||
      filesystem_url.type() != storage::kFileSystemTypeExternal ||
      storage_domain != filesystem_url.origin().host()) {
    return false;
  }

  const base::FilePath& virtual_path = filesystem_url.path();
  if (virtual_path.ReferencesParent())
    return false;
  std::vector<base::FilePath::StringType> components;
  virtual_path.GetComponents(&components);
  if (components.empty())
    return false;
  std::string mount_point = base::FilePath(components[0]).AsUTF8Unsafe();
  if (!StartsWithASCII(mount_point, kMediaGalleryMountPrefix, true))
    return false;

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(url_request);
  if (!request_info)
    return false;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AttemptAutoMountOnUIThread, request_info->GetChildID(),
                 request_info->GetRouteID(), storage_domain, mount_point,
                 callback));
  return true;
}

bool MediaFileSystemBackend::CanHandleType(storage::FileSystemType type) const {
  switch (type) {
    case storage::kFileSystemTypeNativeMedia:
    case storage::kFileSystemTypeDeviceMedia:
#if defined(OS_WIN) || defined(OS_MACOSX)
    case storage::kFileSystemTypePicasa:
    case storage::kFileSystemTypeItunes:
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
#if defined(OS_MACOSX)
    case storage::kFileSystemTypeIphoto:
#endif  // defined(OS_MACOSX)
      return true;
    default:
      return false;
  }
}

void MediaFileSystemBackend::Initialize(storage::FileSystemContext* context) {
}

void MediaFileSystemBackend::ResolveURL(
    const FileSystemURL& url,
    storage::OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  // We never allow opening a new FileSystem via usual ResolveURL.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 GURL(),
                 std::string(),
                 base::File::FILE_ERROR_SECURITY));
}

storage::AsyncFileUtil* MediaFileSystemBackend::GetAsyncFileUtil(
    storage::FileSystemType type) {
  switch (type) {
    case storage::kFileSystemTypeNativeMedia:
      return native_media_file_util_.get();
    case storage::kFileSystemTypeDeviceMedia:
      return device_media_async_file_util_.get();
#if defined(OS_WIN) || defined(OS_MACOSX)
    case storage::kFileSystemTypeItunes:
      return itunes_file_util_.get();
    case storage::kFileSystemTypePicasa:
      return picasa_file_util_.get();
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
#if defined(OS_MACOSX)
    case storage::kFileSystemTypeIphoto:
      return iphoto_file_util_.get();
#endif  // defined(OS_MACOSX)
    default:
      NOTREACHED();
  }
  return NULL;
}

storage::WatcherManager* MediaFileSystemBackend::GetWatcherManager(
    storage::FileSystemType type) {
  return NULL;
}

storage::CopyOrMoveFileValidatorFactory*
MediaFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    storage::FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  switch (type) {
    case storage::kFileSystemTypeNativeMedia:
    case storage::kFileSystemTypeDeviceMedia:
    case storage::kFileSystemTypeIphoto:
    case storage::kFileSystemTypeItunes:
      if (!media_copy_or_move_file_validator_factory_) {
        *error_code = base::File::FILE_ERROR_SECURITY;
        return NULL;
      }
      return media_copy_or_move_file_validator_factory_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

storage::FileSystemOperation* MediaFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  scoped_ptr<storage::FileSystemOperationContext> operation_context(
      new storage::FileSystemOperationContext(context,
                                              media_task_runner_.get()));
  return storage::FileSystemOperation::Create(
      url, context, operation_context.Pass());
}

bool MediaFileSystemBackend::SupportsStreaming(
    const storage::FileSystemURL& url) const {
  if (url.type() == storage::kFileSystemTypeDeviceMedia) {
    DCHECK(device_media_async_file_util_);
    return device_media_async_file_util_->SupportsStreaming(url);
  }

  return false;
}

bool MediaFileSystemBackend::HasInplaceCopyImplementation(
    storage::FileSystemType type) const {
  DCHECK(type == storage::kFileSystemTypeNativeMedia ||
         type == storage::kFileSystemTypeDeviceMedia ||
         type == storage::kFileSystemTypeItunes ||
         type == storage::kFileSystemTypePicasa ||
         type == storage::kFileSystemTypeIphoto);
  return true;
}

scoped_ptr<storage::FileStreamReader>
MediaFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    int64 max_bytes_to_read,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  if (url.type() == storage::kFileSystemTypeDeviceMedia) {
    DCHECK(device_media_async_file_util_);
    scoped_ptr<storage::FileStreamReader> reader =
        device_media_async_file_util_->GetFileStreamReader(
            url, offset, expected_modification_time, context);
    DCHECK(reader);
    return reader.Pass();
  }

  return scoped_ptr<storage::FileStreamReader>(
      storage::FileStreamReader::CreateForLocalFile(
          context->default_file_task_runner(),
          url.path(),
          offset,
          expected_modification_time));
}

scoped_ptr<storage::FileStreamWriter>
MediaFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<storage::FileStreamWriter>(
      storage::FileStreamWriter::CreateForLocalFile(
          context->default_file_task_runner(),
          url.path(),
          offset,
          storage::FileStreamWriter::OPEN_EXISTING_FILE));
}

storage::FileSystemQuotaUtil* MediaFileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

const storage::UpdateObserverList* MediaFileSystemBackend::GetUpdateObservers(
    storage::FileSystemType type) const {
  return NULL;
}

const storage::ChangeObserverList* MediaFileSystemBackend::GetChangeObservers(
    storage::FileSystemType type) const {
  return NULL;
}

const storage::AccessObserverList* MediaFileSystemBackend::GetAccessObservers(
    storage::FileSystemType type) const {
  return NULL;
}
