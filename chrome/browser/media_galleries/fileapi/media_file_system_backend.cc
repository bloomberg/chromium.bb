// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_validator_factory.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/media_galleries/fileapi/itunes_file_util.h"
#include "chrome/browser/media_galleries/fileapi/picasa_file_util.h"
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_MACOSX)
#include "chrome/browser/media_galleries/fileapi/iphoto_file_util.h"
#endif  // defined(OS_MACOSX)

using fileapi::FileSystemContext;
using fileapi::FileSystemURL;

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
          DeviceMediaAsyncFileUtil::Create(profile_path_))
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

bool MediaFileSystemBackend::CanHandleType(
    fileapi::FileSystemType type) const {
  switch (type) {
    case fileapi::kFileSystemTypeNativeMedia:
    case fileapi::kFileSystemTypeDeviceMedia:
#if defined(OS_WIN) || defined(OS_MACOSX)
    case fileapi::kFileSystemTypePicasa:
    case fileapi::kFileSystemTypeItunes:
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
#if defined(OS_MACOSX)
    case fileapi::kFileSystemTypeIphoto:
#endif  // defined(OS_MACOSX)
      return true;
    default:
      return false;
  }
}

void MediaFileSystemBackend::Initialize(fileapi::FileSystemContext* context) {
}

void MediaFileSystemBackend::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    fileapi::OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  // We never allow opening a new isolated FileSystem via usual OpenFileSystem.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 GetFileSystemRootURI(origin_url, type),
                 GetFileSystemName(origin_url, type),
                 base::PLATFORM_FILE_ERROR_SECURITY));
}

fileapi::AsyncFileUtil* MediaFileSystemBackend::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  switch (type) {
    case fileapi::kFileSystemTypeNativeMedia:
      return native_media_file_util_.get();
    case fileapi::kFileSystemTypeDeviceMedia:
      return device_media_async_file_util_.get();
#if defined(OS_WIN) || defined(OS_MACOSX)
    case fileapi::kFileSystemTypeItunes:
      return itunes_file_util_.get();
    case fileapi::kFileSystemTypePicasa:
      return picasa_file_util_.get();
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
#if defined(OS_MACOSX)
    case fileapi::kFileSystemTypeIphoto:
      return iphoto_file_util_.get();
#endif  // defined(OS_MACOSX)
    default:
      NOTREACHED();
  }
  return NULL;
}

fileapi::CopyOrMoveFileValidatorFactory*
MediaFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    fileapi::FileSystemType type, base::PlatformFileError* error_code) {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  switch (type) {
    case fileapi::kFileSystemTypeNativeMedia:
    case fileapi::kFileSystemTypeDeviceMedia:
    case fileapi::kFileSystemTypeIphoto:
    case fileapi::kFileSystemTypeItunes:
      if (!media_copy_or_move_file_validator_factory_) {
        *error_code = base::PLATFORM_FILE_ERROR_SECURITY;
        return NULL;
      }
      return media_copy_or_move_file_validator_factory_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

fileapi::FileSystemOperation*
MediaFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  scoped_ptr<fileapi::FileSystemOperationContext> operation_context(
      new fileapi::FileSystemOperationContext(
          context, media_task_runner_.get()));
  return fileapi::FileSystemOperation::Create(
      url, context, operation_context.Pass());
}

scoped_ptr<webkit_blob::FileStreamReader>
MediaFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>(
      webkit_blob::FileStreamReader::CreateForLocalFile(
          context->default_file_task_runner(),
          url.path(), offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
MediaFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<fileapi::FileStreamWriter>(
      fileapi::FileStreamWriter::CreateForLocalFile(
          context->default_file_task_runner(),
          url.path(), offset));
}

fileapi::FileSystemQuotaUtil*
MediaFileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return NULL;
}
