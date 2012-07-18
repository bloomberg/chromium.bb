// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system_proxy.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"

using base::MessageLoopProxy;
using content::BrowserThread;
using fileapi::FileSystemURL;
using fileapi::FileSystemOperationInterface;
using webkit_blob::ShareableFileReference;

namespace {

const char kGDataRootDirectory[] = "drive";
const char kFeedField[] = "feed";

// Helper function that creates platform file on bocking IO thread pool.
void OpenPlatformFileOnIOPool(const FilePath& local_path,
                                int file_flags,
                                base::PlatformFile* platform_file,
                                base::PlatformFileError* open_error) {
  bool created;
  *platform_file = base::CreatePlatformFile(local_path,
                                            file_flags,
                                            &created,
                                            open_error);
}

// Helper function to run reply on results of OpenPlatformFileOnIOPool() on
// IO thread.
void OnPlatformFileOpened(
    const FileSystemOperationInterface::OpenFileCallback& callback,
    base::ProcessHandle peer_handle,
    base::PlatformFile* platform_file,
    base::PlatformFileError* open_error) {
  callback.Run(*open_error, *platform_file, peer_handle);
}

// Helper function to run OpenFileCallback from
// GDataFileSystemProxy::OpenFile().
void OnGetFileByPathForOpen(
    const FileSystemOperationInterface::OpenFileCallback& callback,
    int file_flags,
    base::ProcessHandle peer_handle,
    gdata::GDataFileError gdata_error,
    const FilePath& local_path,
    const std::string& unused_mime_type,
    gdata::GDataFileType file_type) {
  base::PlatformFileError error =
      gdata::util::GDataFileErrorToPlatformError(gdata_error);
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  base::PlatformFile* platform_file = new base::PlatformFile(
      base::kInvalidPlatformFileValue);
  base::PlatformFileError* open_error =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
      base::Bind(&OpenPlatformFileOnIOPool,
                 local_path,
                 file_flags,
                 platform_file,
                 open_error),
      base::Bind(&OnPlatformFileOpened,
                 callback,
                 peer_handle,
                 base::Owned(platform_file),
                 base::Owned(open_error)));

}

// Helper function to run SnapshotFileCallback from
// GDataFileSystemProxy::CreateSnapshotFile().
void CallSnapshotFileCallback(
    const FileSystemOperationInterface::SnapshotFileCallback& callback,
    const base::PlatformFileInfo& file_info,
    gdata::GDataFileError gdata_error,
    const FilePath& local_path,
    const std::string& unused_mime_type,
    gdata::GDataFileType file_type) {
  scoped_refptr<ShareableFileReference> file_ref;
  base::PlatformFileError error =
      gdata::util::GDataFileErrorToPlatformError(gdata_error);

  // If the file is a hosted document, a temporary JSON file is created to
  // represent the document. The JSON file is not cached and its lifetime
  // is managed by ShareableFileReference.
  if (error == base::PLATFORM_FILE_OK && file_type == gdata::HOSTED_DOCUMENT) {
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

void OnClose(const FilePath& local_path, gdata::GDataFileError error_code) {
  DVLOG(1) << "Closed: " << local_path.AsUTF8Unsafe() << ": " << error_code;
}

void DoTruncateOnFileThread(
    const FilePath& local_cache_path,
    int64 length,
    base::PlatformFileError* result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::PlatformFile file = base::CreatePlatformFile(
      local_cache_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
      NULL,
      result);
  if (*result == base::PLATFORM_FILE_OK) {
    DCHECK_NE(base::kInvalidPlatformFileValue, file);
    if (!base::TruncatePlatformFile(file, length))
      *result = base::PLATFORM_FILE_ERROR_FAILED;
    base::ClosePlatformFile(file);
  }
}

void DidCloseFileForTruncate(
    const FileSystemOperationInterface::StatusCallback& callback,
    base::PlatformFileError truncate_result,
    gdata::GDataFileError close_result) {
  // Reports the first error.
  callback.Run(truncate_result == base::PLATFORM_FILE_OK ?
               gdata::util::GDataFileErrorToPlatformError(close_result) :
               truncate_result);
}

}  // namespace

namespace gdata {

base::FileUtilProxy::Entry GDataEntryProtoToFileUtilProxyEntry(
    const GDataEntryProto& proto) {
  base::PlatformFileInfo file_info;
  GDataEntry::ConvertProtoToPlatformFileInfo(proto.file_info(), &file_info);

  base::FileUtilProxy::Entry entry;
  entry.name = proto.file_name();
  entry.is_directory = file_info.is_directory;
  entry.size = file_info.size;
  entry.last_modified_time = file_info.last_modified;
  return entry;
}

// GDataFileSystemProxy class implementation.

GDataFileSystemProxy::GDataFileSystemProxy(
    GDataFileSystemInterface* file_system)
    : file_system_(file_system) {
  // Should be created from the file browser extension API (AddMountFunction)
  // on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void GDataFileSystemProxy::GetFileInfo(const FileSystemURL& file_url,
    const FileSystemOperationInterface::GetMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::PlatformFileInfo(),
                    FilePath()));
    return;
  }

  file_system_->GetEntryInfoByPath(
      file_path,
      base::Bind(&GDataFileSystemProxy::OnGetMetadata,
                 this,
                 file_path,
                 callback));
}

void GDataFileSystemProxy::Copy(const FileSystemURL& src_file_url,
    const FileSystemURL& dest_file_url,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Copy(
      src_file_path,
      dest_file_path,
      base::Bind(&GDataFileSystemProxy::OnStatusCallback, this, callback));
}

void GDataFileSystemProxy::Move(const FileSystemURL& src_file_url,
    const FileSystemURL& dest_file_url,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Move(
      src_file_path,
      dest_file_path,
      base::Bind(&GDataFileSystemProxy::OnStatusCallback, this, callback));
}

void GDataFileSystemProxy::ReadDirectory(const FileSystemURL& file_url,
    const FileSystemOperationInterface::ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   std::vector<base::FileUtilProxy::Entry>(),
                   false));
    return;
  }

  file_system_->ReadDirectoryByPath(
      file_path,
      base::Bind(&GDataFileSystemProxy::OnReadDirectory,
                 this,
                 callback));
}

void GDataFileSystemProxy::Remove(const FileSystemURL& file_url, bool recursive,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Remove(
      file_path,
      recursive,
      base::Bind(&GDataFileSystemProxy::OnStatusCallback, this, callback));
}

void GDataFileSystemProxy::CreateDirectory(
    const FileSystemURL& file_url,
    bool exclusive,
    bool recursive,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->CreateDirectory(
      file_path,
      exclusive,
      recursive,
      base::Bind(&GDataFileSystemProxy::OnStatusCallback, this, callback));
}

void GDataFileSystemProxy::CreateFile(
    const FileSystemURL& file_url,
    bool exclusive,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->CreateFile(
      file_path,
      exclusive,
      base::Bind(&GDataFileSystemProxy::OnStatusCallback, this, callback));
}

void GDataFileSystemProxy::Truncate(
    const FileSystemURL& file_url, int64 length,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (length < 0) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
    return;
  }

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  // TODO(kinaba): http://crbug.com/132780.
  // Optimize the cases for small |length|, at least for |length| == 0.
  // CreateWritableSnapshotFile downloads the whole content unnecessarily.
  file_system_->OpenFile(
      file_path,
      base::Bind(&GDataFileSystemProxy::OnFileOpenedForTruncate,
                 this,
                 file_path,
                 length,
                 callback));
}

void GDataFileSystemProxy::OnOpenFileForWriting(
    int file_flags,
    base::ProcessHandle peer_handle,
    const FileSystemOperationInterface::OpenFileCallback& callback,
    GDataFileError gdata_error,
    const FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::PlatformFileError error =
      gdata::util::GDataFileErrorToPlatformError(gdata_error);

  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  // Cache file prepared for modification is available. Truncate it.
  // File operation must be done on FILE thread, so relay the operation.
  base::PlatformFileError* result =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  base::PlatformFile* platform_file = new base::PlatformFile(
      base::kInvalidPlatformFileValue);
  bool posted = BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
        base::Bind(&OpenPlatformFileOnIOPool,
                   local_cache_path,
                   file_flags,
                   platform_file,
                   result),
        base::Bind(&OnPlatformFileOpened,
                   callback,
                   peer_handle,
                   base::Owned(platform_file),
                   base::Owned(result)));
  DCHECK(posted);
}

void GDataFileSystemProxy::OnCreateFileForOpen(
    const FilePath& file_path,
    int file_flags,
    base::ProcessHandle peer_handle,
    const FileSystemOperationInterface::OpenFileCallback& callback,
    GDataFileError gdata_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError create_result =
      gdata::util::GDataFileErrorToPlatformError(gdata_error);

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
  file_system_->OpenFile(
      file_path,
      base::Bind(&GDataFileSystemProxy::OnOpenFileForWriting,
                 this,
                 file_flags,
                 peer_handle,
                 callback));
}

void GDataFileSystemProxy::OnFileOpenedForTruncate(
    const FilePath& virtual_path,
    int64 length,
    const fileapi::FileSystemOperationInterface::StatusCallback& callback,
    GDataFileError open_result,
    const FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (open_result != GDATA_FILE_OK) {
    callback.Run(util::GDataFileErrorToPlatformError(open_result));
    return;
  }

  // Cache file prepared for modification is available. Truncate it.
  // File operation must be done on FILE thread, so relay the operation.
  base::PlatformFileError* result =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  bool posted = BrowserThread::GetMessageLoopProxyForThread(
      BrowserThread::FILE)->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&DoTruncateOnFileThread,
                     local_cache_path,
                     length,
                     result),
          base::Bind(&GDataFileSystemProxy::DidTruncate,
                     this,
                     virtual_path,
                     callback,
                     base::Owned(result)));
  DCHECK(posted);
}

void GDataFileSystemProxy::DidTruncate(
    const FilePath& virtual_path,
    const FileSystemOperationInterface::StatusCallback& callback,
    base::PlatformFileError* truncate_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Truncation finished. We must close the file no matter |truncate_result|
  // indicates an error or not.
  file_system_->CloseFile(
      virtual_path,
      base::Bind(&DidCloseFileForTruncate,
                 callback,
                 base::PlatformFileError(*truncate_result)));
}

void GDataFileSystemProxy::OpenFile(
    const FileSystemURL& file_url,
    int file_flags,
    base::ProcessHandle peer_handle,
    const FileSystemOperationInterface::OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
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
      file_system_->OpenFile(
          file_path,
          base::Bind(&GDataFileSystemProxy::OnOpenFileForWriting,
                     this,
                     file_flags,
                     peer_handle,
                     callback));
    } else {
      // Read-only file open.
      file_system_->GetFileByPath(file_path,
                                  base::Bind(&OnGetFileByPathForOpen,
                                             callback,
                                             file_flags,
                                             peer_handle),
                                  GetDownloadDataCallback());
    }
  } else if ((file_flags & base::PLATFORM_FILE_CREATE) ||
             (file_flags & base::PLATFORM_FILE_CREATE_ALWAYS)) {
    // Open existing file for writing.
    file_system_->CreateFile(
        file_path,
        file_flags & base::PLATFORM_FILE_EXCLUSIVE_WRITE,
        base::Bind(&GDataFileSystemProxy::OnCreateFileForOpen,
                   this,
                   file_path,
                   file_flags,
                   peer_handle,
                   callback));
  } else {
    NOTREACHED() << "Unhandled file flags combination " << file_flags;
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_FAILED,
                    base::kInvalidPlatformFileValue,
                    peer_handle));
  }
}

void GDataFileSystemProxy::NotifyCloseFile(const FileSystemURL& url) {
  FilePath file_path;
  if (!ValidateUrl(url, &file_path))
    return;

  file_system_->CloseFile(file_path, FileOperationCallback());
}

void GDataFileSystemProxy::CreateSnapshotFile(
    const FileSystemURL& file_url,
    const FileSystemOperationInterface::SnapshotFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::PlatformFileInfo(),
                    FilePath(),
                    scoped_refptr<ShareableFileReference>(NULL)));
    return;
  }

  file_system_->GetEntryInfoByPath(
      file_path,
      base::Bind(&GDataFileSystemProxy::OnGetEntryInfoByPath,
                 this,
                 file_path,
                 callback));
}

void GDataFileSystemProxy::OnGetEntryInfoByPath(
    const FilePath& entry_path,
    const FileSystemOperationInterface::SnapshotFileCallback& callback,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != GDATA_FILE_OK || !entry_proto.get()) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::PlatformFileInfo(),
                    FilePath(),
                    scoped_refptr<ShareableFileReference>(NULL)));
    return;
  }

  base::PlatformFileInfo file_info;
  GDataEntry::ConvertProtoToPlatformFileInfo(
      entry_proto->file_info(),
      &file_info);

  file_system_->GetFileByPath(entry_path,
                              base::Bind(&CallSnapshotFileCallback,
                                         callback,
                                         file_info),
                              GetDownloadDataCallback());
}

void GDataFileSystemProxy::CreateWritableSnapshotFile(
    const FileSystemURL& file_url,
    const fileapi::WritableSnapshotFile& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    FilePath(),
                    scoped_refptr<ShareableFileReference>(NULL)));
    return;
  }

  file_system_->OpenFile(
      file_path,
      base::Bind(&GDataFileSystemProxy::OnCreateWritableSnapshotFile,
                 this,
                 file_path,
                 callback));
}

GDataFileSystemProxy::~GDataFileSystemProxy() {
  // Should be deleted from the CrosMountPointProvider on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

// static.
bool GDataFileSystemProxy::ValidateUrl(
    const FileSystemURL& url, FilePath* file_path) {
  // what platform you're on.
  if (!url.is_valid() || url.type() != fileapi::kFileSystemTypeExternal) {
    return false;
  }
  *file_path = url.path();
  return true;
}

void GDataFileSystemProxy::OnStatusCallback(
    const fileapi::FileSystemOperationInterface::StatusCallback& callback,
    gdata::GDataFileError error) {
  callback.Run(util::GDataFileErrorToPlatformError(error));
}

void GDataFileSystemProxy::OnGetMetadata(
    const FilePath& file_path,
    const FileSystemOperationInterface::GetMetadataCallback& callback,
    GDataFileError error,
    scoped_ptr<gdata::GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != GDATA_FILE_OK) {
    callback.Run(util::GDataFileErrorToPlatformError(error),
                 base::PlatformFileInfo(),
                 FilePath());
    return;
  }
  DCHECK(entry_proto.get());

  base::PlatformFileInfo file_info;
  GDataEntry::ConvertProtoToPlatformFileInfo(
      entry_proto->file_info(),
      &file_info);

  callback.Run(base::PLATFORM_FILE_OK, file_info, file_path);
}

void GDataFileSystemProxy::OnReadDirectory(
    const FileSystemOperationInterface::ReadDirectoryCallback&
    callback,
    GDataFileError error,
    bool hide_hosted_documents,
    scoped_ptr<gdata::GDataDirectoryProto> directory_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != GDATA_FILE_OK) {
    callback.Run(util::GDataFileErrorToPlatformError(error),
                 std::vector<base::FileUtilProxy::Entry>(),
                 false);
    return;
  }
  std::vector<base::FileUtilProxy::Entry> entries;
  // Convert gdata files to something File API stack can understand.
  for (int i = 0; i < directory_proto->child_directories_size(); ++i) {
    const GDataDirectoryProto& proto = directory_proto->child_directories(i);
    entries.push_back(
        GDataEntryProtoToFileUtilProxyEntry(proto.gdata_entry()));
  }
  for (int i = 0; i < directory_proto->child_files_size(); ++i) {
    const GDataFileProto& proto = directory_proto->child_files(i);
    if (hide_hosted_documents && proto.is_hosted_document())
        continue;
    entries.push_back(
        GDataEntryProtoToFileUtilProxyEntry(proto.gdata_entry()));
  }

  callback.Run(base::PLATFORM_FILE_OK, entries, false);
}

void GDataFileSystemProxy::OnCreateWritableSnapshotFile(
    const FilePath& virtual_path,
    const fileapi::WritableSnapshotFile& callback,
    GDataFileError result,
    const FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<ShareableFileReference> file_ref;

  if (result == GDATA_FILE_OK) {
    file_ref = ShareableFileReference::GetOrCreate(
        local_path,
        ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    file_ref->AddFinalReleaseCallback(
        base::Bind(&GDataFileSystemProxy::CloseWritableSnapshotFile,
                   this,
                   virtual_path));
  }

  callback.Run(
      util::GDataFileErrorToPlatformError(result), local_path, file_ref);
}

void GDataFileSystemProxy::CloseWritableSnapshotFile(
    const FilePath& virtual_path,
    const FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  file_system_->CloseFile(virtual_path, base::Bind(&OnClose, virtual_path));
}

}  // namespace gdata
