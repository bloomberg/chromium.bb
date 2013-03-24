// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_system_proxy.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/google_apis/time_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"

using base::MessageLoopProxy;
using content::BrowserThread;
using fileapi::FileSystemURL;
using fileapi::FileSystemOperation;
using webkit_blob::ShareableFileReference;

namespace drive {

namespace {

const char kFeedField[] = "feed";

// Helper function to run reply on results of base::CreatePlatformFile() on
// IO thread.
void OnPlatformFileOpened(
    const FileSystemOperation::OpenFileCallback& callback,
    base::ProcessHandle peer_handle,
    base::PlatformFileError* open_error,
    base::PlatformFile platform_file) {
  callback.Run(*open_error, platform_file, peer_handle);
}

// Helper function to run OpenFileCallback from
// DriveFileSystemProxy::OpenFile().
void OnGetFileByPathForOpen(
    const FileSystemOperation::OpenFileCallback& callback,
    int file_flags,
    base::ProcessHandle peer_handle,
    DriveFileError file_error,
    const base::FilePath& local_path,
    const std::string& unused_mime_type,
    DriveFileType file_type) {
  base::PlatformFileError error =
      DriveFileErrorToPlatformError(file_error);
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  base::PlatformFileError* open_error =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&base::CreatePlatformFile,
                 local_path,
                 file_flags,
                 static_cast<bool*>(NULL),
                 open_error),
      base::Bind(&OnPlatformFileOpened,
                 callback,
                 peer_handle,
                 base::Owned(open_error)));
}

// Helper function to run SnapshotFileCallback from
// DriveFileSystemProxy::CreateSnapshotFile().
void CallSnapshotFileCallback(
    const FileSystemOperation::SnapshotFileCallback& callback,
    const base::PlatformFileInfo& file_info,
    DriveFileError file_error,
    const base::FilePath& local_path,
    const std::string& unused_mime_type,
    DriveFileType file_type) {
  scoped_refptr<ShareableFileReference> file_ref;
  base::PlatformFileError error =
      DriveFileErrorToPlatformError(file_error);

  // If the file is a hosted document, a temporary JSON file is created to
  // represent the document. The JSON file is not cached and its lifetime
  // is managed by ShareableFileReference.
  if (error == base::PLATFORM_FILE_OK && file_type == HOSTED_DOCUMENT) {
    file_ref = ShareableFileReference::GetOrCreate(
        local_path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  }

  // When reading file, last modified time specified in file info will be
  // compared to the last modified time of the local version of the drive file.
  // Since those two values don't generally match (last modification time on the
  // drive server vs. last modification time of the local, downloaded file), so
  // we have to opt out from this check. We do this by unsetting last_modified
  // value in the file info passed to the CreateSnapshot caller.
  base::PlatformFileInfo final_file_info(file_info);
  final_file_info.last_modified = base::Time();

  callback.Run(error, final_file_info, local_path, file_ref);
}

// Emits debug log when DriveFileSystem::CloseFile() is complete.
void EmitDebugLogForCloseFile(const base::FilePath& local_path,
                              DriveFileError file_error) {
  DVLOG(1) << "Closed: " << local_path.AsUTF8Unsafe() << ": " << file_error;
}

base::PlatformFileError DoTruncateOnBlockingPool(
    const base::FilePath& local_cache_path,
    int64 length) {
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;

  base::PlatformFile file = base::CreatePlatformFile(
      local_cache_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
      NULL,
      &result);
  if (result == base::PLATFORM_FILE_OK) {
    DCHECK_NE(base::kInvalidPlatformFileValue, file);
    if (!base::TruncatePlatformFile(file, length))
      result = base::PLATFORM_FILE_ERROR_FAILED;
    base::ClosePlatformFile(file);
  }
  return result;
}

void DidCloseFileForTruncate(
    const FileSystemOperation::StatusCallback& callback,
    base::PlatformFileError truncate_result,
    DriveFileError close_result) {
  // Reports the first error.
  callback.Run(truncate_result == base::PLATFORM_FILE_OK ?
               DriveFileErrorToPlatformError(close_result) :
               truncate_result);
}

}  // namespace

base::FileUtilProxy::Entry DriveEntryProtoToFileUtilProxyEntry(
    const DriveEntryProto& proto) {
  base::PlatformFileInfo file_info;
  util::ConvertProtoToPlatformFileInfo(proto.file_info(), &file_info);

  base::FileUtilProxy::Entry entry;
  entry.name = proto.base_name();
  entry.is_directory = file_info.is_directory;
  entry.size = file_info.size;
  entry.last_modified_time = file_info.last_modified;
  return entry;
}

// DriveFileSystemProxy class implementation.

DriveFileSystemProxy::DriveFileSystemProxy(
    DriveFileSystemInterface* file_system)
    : file_system_(file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DriveFileSystemProxy::DetachFromFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_ = NULL;
}

void DriveFileSystemProxy::GetFileInfo(const FileSystemURL& file_url,
    const FileSystemOperation::GetMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::PlatformFileInfo(),
                    base::FilePath()));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::GetEntryInfoByPath,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnGetMetadata,
                                this,
                                file_path,
                                callback))));
}

void DriveFileSystemProxy::Copy(const FileSystemURL& src_file_url,
    const FileSystemURL& dest_file_url,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::Copy,
                 base::Unretained(file_system_),
                 src_file_path,
                 dest_file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void DriveFileSystemProxy::Move(const FileSystemURL& src_file_url,
    const FileSystemURL& dest_file_url,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::Move,
                 base::Unretained(file_system_),
                 src_file_path,
                 dest_file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void DriveFileSystemProxy::ReadDirectory(const FileSystemURL& file_url,
    const FileSystemOperation::ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   std::vector<base::FileUtilProxy::Entry>(),
                   false));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::ReadDirectoryByPath,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnReadDirectory,
                                this,
                                callback))));
}

void DriveFileSystemProxy::Remove(const FileSystemURL& file_url, bool recursive,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::Remove,
                 base::Unretained(file_system_),
                 file_path,
                 recursive,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void DriveFileSystemProxy::CreateDirectory(
    const FileSystemURL& file_url,
    bool exclusive,
    bool recursive,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::CreateDirectory,
                 base::Unretained(file_system_),
                 file_path,
                 exclusive,
                 recursive,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void DriveFileSystemProxy::CreateFile(
    const FileSystemURL& file_url,
    bool exclusive,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::CreateFile,
                 base::Unretained(file_system_),
                 file_path,
                 exclusive,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnStatusCallback,
                                this,
                                callback))));
}

void DriveFileSystemProxy::Truncate(
    const FileSystemURL& file_url, int64 length,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (length < 0) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
    return;
  }

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  // TODO(kinaba): http://crbug.com/132780.
  // Optimize the cases for small |length|, at least for |length| == 0.
  // CreateWritableSnapshotFile downloads the whole content unnecessarily.
  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::OpenFile,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnFileOpenedForTruncate,
                                this,
                                file_path,
                                length,
                                callback))));
}

void DriveFileSystemProxy::OnOpenFileForWriting(
    int file_flags,
    base::ProcessHandle peer_handle,
    const FileSystemOperation::OpenFileCallback& callback,
    DriveFileError file_error,
    const base::FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::PlatformFileError error =
      DriveFileErrorToPlatformError(file_error);

  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  // Cache file prepared for modification is available. Truncate it.
  base::PlatformFileError* result =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  bool posted = base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&base::CreatePlatformFile,
                 local_cache_path,
                 file_flags,
                 static_cast<bool*>(NULL),
                 result),
      base::Bind(&OnPlatformFileOpened,
                 callback,
                 peer_handle,
                 base::Owned(result)));
  DCHECK(posted);
}

void DriveFileSystemProxy::OnCreateFileForOpen(
    const base::FilePath& file_path,
    int file_flags,
    base::ProcessHandle peer_handle,
    const FileSystemOperation::OpenFileCallback& callback,
    DriveFileError file_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError create_result =
      DriveFileErrorToPlatformError(file_error);

  if ((create_result == base::PLATFORM_FILE_OK) ||
      ((create_result == base::PLATFORM_FILE_ERROR_EXISTS) &&
       (file_flags & base::PLATFORM_FILE_CREATE_ALWAYS))) {
    // If we are trying to always create an existing file, then
    // if it really exists open it as truncated.
    file_flags &= ~base::PLATFORM_FILE_CREATE;
    file_flags &= ~base::PLATFORM_FILE_CREATE_ALWAYS;
    file_flags |= base::PLATFORM_FILE_OPEN_TRUNCATED;
  } else {
    callback.Run(create_result, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  // Open created (or existing) file for writing.
  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::OpenFile,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnOpenFileForWriting,
                                this,
                                file_flags,
                                peer_handle,
                                callback))));
}

void DriveFileSystemProxy::OnFileOpenedForTruncate(
    const base::FilePath& virtual_path,
    int64 length,
    const fileapi::FileSystemOperation::StatusCallback& callback,
    DriveFileError open_result,
    const base::FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (open_result != DRIVE_FILE_OK) {
    callback.Run(DriveFileErrorToPlatformError(open_result));
    return;
  }

  // Cache file prepared for modification is available. Truncate it.
  bool posted = base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&DoTruncateOnBlockingPool,
                 local_cache_path,
                 length),
      base::Bind(&DriveFileSystemProxy::DidTruncate,
                 this,
                 virtual_path,
                 callback));
  DCHECK(posted);
}

void DriveFileSystemProxy::DidTruncate(
    const base::FilePath& virtual_path,
    const FileSystemOperation::StatusCallback& callback,
    base::PlatformFileError truncate_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Truncation finished. We must close the file no matter |truncate_result|
  // indicates an error or not.
  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::CloseFile,
                 base::Unretained(file_system_),
                 virtual_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DidCloseFileForTruncate,
                                callback,
                                truncate_result))));
}

void DriveFileSystemProxy::OpenFile(
    const FileSystemURL& file_url,
    int file_flags,
    base::ProcessHandle peer_handle,
    const FileSystemOperation::OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::kInvalidPlatformFileValue,
                    peer_handle));
    return;
  }

  // TODO(zelidrag): Wire all other file open operations.
  if ((file_flags & base::PLATFORM_FILE_DELETE_ON_CLOSE)) {
    NOTIMPLEMENTED() << "File create/write operations not yet supported "
                     << file_path.value();
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_FAILED,
                    base::kInvalidPlatformFileValue,
                    peer_handle));
    return;
  }

  if ((file_flags & base::PLATFORM_FILE_OPEN) ||
      (file_flags & base::PLATFORM_FILE_OPEN_ALWAYS) ||
      (file_flags & base::PLATFORM_FILE_OPEN_TRUNCATED)) {
    if ((file_flags & base::PLATFORM_FILE_OPEN_TRUNCATED) ||
        (file_flags & base::PLATFORM_FILE_OPEN_ALWAYS) ||
        (file_flags & base::PLATFORM_FILE_WRITE) ||
        (file_flags & base::PLATFORM_FILE_EXCLUSIVE_WRITE)) {
      // Open existing file for writing.
      CallDriveFileSystemMethodOnUIThread(
          base::Bind(&DriveFileSystemInterface::OpenFile,
                     base::Unretained(file_system_),
                     file_path,
                     google_apis::CreateRelayCallback(
                         base::Bind(&DriveFileSystemProxy::OnOpenFileForWriting,
                                    this,
                                    file_flags,
                                    peer_handle,
                                    callback))));
    } else {
      // Read-only file open.
      CallDriveFileSystemMethodOnUIThread(
          base::Bind(&DriveFileSystemInterface::GetFileByPath,
                     base::Unretained(file_system_),
                     file_path,
                     google_apis::CreateRelayCallback(
                         base::Bind(&OnGetFileByPathForOpen,
                                    callback,
                                    file_flags,
                                    peer_handle))));
    }
  } else if ((file_flags & base::PLATFORM_FILE_CREATE) ||
             (file_flags & base::PLATFORM_FILE_CREATE_ALWAYS)) {
    // Open existing file for writing.
    CallDriveFileSystemMethodOnUIThread(
        base::Bind(&DriveFileSystemInterface::CreateFile,
                   base::Unretained(file_system_),
                   file_path,
                   file_flags & base::PLATFORM_FILE_EXCLUSIVE_WRITE,
                   google_apis::CreateRelayCallback(
                       base::Bind(&DriveFileSystemProxy::OnCreateFileForOpen,
                                  this,
                                  file_path,
                                  file_flags,
                                  peer_handle,
                                  callback))));
  } else {
    NOTREACHED() << "Unhandled file flags combination " << file_flags;
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_FAILED,
                    base::kInvalidPlatformFileValue,
                    peer_handle));
  }
}

void DriveFileSystemProxy::NotifyCloseFile(const FileSystemURL& url) {
  base::FilePath file_path;
  if (!ValidateUrl(url, &file_path))
    return;

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::CloseFile,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&EmitDebugLogForCloseFile,
                                file_path))));
}

void DriveFileSystemProxy::TouchFile(
    const fileapi::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // TODO(kinaba,kochi): crbug.com/144369. Support this operations once we have
  // migrated to the new Drive API.
  MessageLoopProxy::current()->PostTask(FROM_HERE,
       base::Bind(callback, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
}

void DriveFileSystemProxy::CreateSnapshotFile(
    const FileSystemURL& file_url,
    const FileSystemOperation::SnapshotFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::PlatformFileInfo(),
                    base::FilePath(),
                    scoped_refptr<ShareableFileReference>(NULL)));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::GetEntryInfoByPath,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&DriveFileSystemProxy::OnGetEntryInfoByPath,
                                this,
                                file_path,
                                callback))));
}

void DriveFileSystemProxy::OnGetEntryInfoByPath(
    const base::FilePath& entry_path,
    const FileSystemOperation::SnapshotFileCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != DRIVE_FILE_OK || !entry_proto.get()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 base::PlatformFileInfo(),
                 base::FilePath(),
                 scoped_refptr<ShareableFileReference>(NULL));
    return;
  }

  base::PlatformFileInfo file_info;
  util::ConvertProtoToPlatformFileInfo(entry_proto->file_info(), &file_info);

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::GetFileByPath,
                 base::Unretained(file_system_),
                 entry_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&CallSnapshotFileCallback,
                                callback,
                                file_info))));
}

void DriveFileSystemProxy::CreateWritableSnapshotFile(
    const FileSystemURL& file_url,
    const fileapi::WritableSnapshotFile& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::FilePath(),
                    scoped_refptr<ShareableFileReference>(NULL)));
    return;
  }

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::OpenFile,
                 base::Unretained(file_system_),
                 file_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(
                         &DriveFileSystemProxy::OnCreateWritableSnapshotFile,
                         this,
                         file_path,
                         callback))));
}

DriveFileSystemProxy::~DriveFileSystemProxy() {
  // Should be deleted from the CrosMountPointProvider on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

// static.
bool DriveFileSystemProxy::ValidateUrl(
    const FileSystemURL& url, base::FilePath* file_path) {
  // what platform you're on.
  if (!url.is_valid() || url.type() != fileapi::kFileSystemTypeDrive)
    return false;

  // |url.virtual_path()| cannot be used directly because in the case the url is
  // isolated file system url, the virtual path will be formatted as
  // <isolated_file_system_id>/<file_name> and thus unusable by drive file
  // system.
  // TODO(kinaba): fix other uses of virtual_path() as in
  // https://codereview.chromium.org/12483010/
  *file_path = util::ExtractDrivePath(url.path());
  return !file_path->empty();
}

void DriveFileSystemProxy::CallDriveFileSystemMethodOnUIThread(
    const base::Closure& method_call) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DriveFileSystemProxy::CallDriveFileSystemMethodOnUIThreadInternal,
          this,
          method_call));
}

void DriveFileSystemProxy::CallDriveFileSystemMethodOnUIThreadInternal(
    const base::Closure& method_call) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If |file_system_| is NULL, it means the file system has already shut down.
  if (file_system_)
    method_call.Run();
}

void DriveFileSystemProxy::OnStatusCallback(
    const fileapi::FileSystemOperation::StatusCallback& callback,
    DriveFileError error) {
  callback.Run(DriveFileErrorToPlatformError(error));
}

void DriveFileSystemProxy::OnGetMetadata(
    const base::FilePath& file_path,
    const FileSystemOperation::GetMetadataCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != DRIVE_FILE_OK) {
    callback.Run(DriveFileErrorToPlatformError(error),
                 base::PlatformFileInfo(),
                 base::FilePath());
    return;
  }
  DCHECK(entry_proto.get());

  base::PlatformFileInfo file_info;
  util::ConvertProtoToPlatformFileInfo(entry_proto->file_info(), &file_info);

  callback.Run(base::PLATFORM_FILE_OK, file_info, file_path);
}

void DriveFileSystemProxy::OnReadDirectory(
    const FileSystemOperation::ReadDirectoryCallback&
    callback,
    DriveFileError error,
    bool hide_hosted_documents,
    scoped_ptr<DriveEntryProtoVector> proto_entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != DRIVE_FILE_OK) {
    callback.Run(DriveFileErrorToPlatformError(error),
                 std::vector<base::FileUtilProxy::Entry>(),
                 false);
    return;
  }
  DCHECK(proto_entries.get());

  std::vector<base::FileUtilProxy::Entry> entries;
  // Convert Drive files to something File API stack can understand.
  for (size_t i = 0; i < proto_entries->size(); ++i) {
    const DriveEntryProto& proto = (*proto_entries)[i];
    if (proto.has_file_specific_info() &&
        proto.file_specific_info().is_hosted_document() &&
        hide_hosted_documents) {
      continue;
    }
    entries.push_back(DriveEntryProtoToFileUtilProxyEntry(proto));
  }

  callback.Run(base::PLATFORM_FILE_OK, entries, false);
}

void DriveFileSystemProxy::OnCreateWritableSnapshotFile(
    const base::FilePath& virtual_path,
    const fileapi::WritableSnapshotFile& callback,
    DriveFileError result,
    const base::FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<ShareableFileReference> file_ref;

  if (result == DRIVE_FILE_OK) {
    file_ref = ShareableFileReference::GetOrCreate(
        local_path,
        ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    file_ref->AddFinalReleaseCallback(
        base::Bind(&DriveFileSystemProxy::CloseWritableSnapshotFile,
                   this,
                   virtual_path));
  }

  callback.Run(DriveFileErrorToPlatformError(result), local_path, file_ref);
}

void DriveFileSystemProxy::CloseWritableSnapshotFile(
    const base::FilePath& virtual_path,
    const base::FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CallDriveFileSystemMethodOnUIThread(
      base::Bind(&DriveFileSystemInterface::CloseFile,
                 base::Unretained(file_system_),
                 virtual_path,
                 google_apis::CreateRelayCallback(
                     base::Bind(&EmitDebugLogForCloseFile,
                                virtual_path))));
}

}  // namespace drive
