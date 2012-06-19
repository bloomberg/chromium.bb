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
#include "content/public/browser/browser_thread.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using base::MessageLoopProxy;
using content::BrowserThread;
using fileapi::FileSystemOperationInterface;
using webkit_blob::ShareableFileReference;

namespace {

const char kGDataRootDirectory[] = "drive";
const char kFeedField[] = "feed";


// Helper function that creates platform file on bocking IO thread pool.
void CreatePlatformFileOnIOPool(const FilePath& local_path,
                                int file_flags,
                                base::PlatformFile* platform_file,
                                base::PlatformFileError* open_error) {
  bool created;
  *platform_file = base::CreatePlatformFile(local_path,
                                            file_flags,
                                            &created,
                                            open_error);
}

// Helper function to run reply on results of CreatePlatformFileOnIOPool() on
// IO thread.
void OnPlatformFileCreated(
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
    base::PlatformFileError error,
    const FilePath& local_path,
    const std::string& unused_mime_type,
    gdata::GDataFileType file_type) {
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, base::kInvalidPlatformFileValue, peer_handle);
    return;
  }

  base::PlatformFile* platform_file = new base::PlatformFile(
      base::kInvalidPlatformFileValue);
  base::PlatformFileError* open_error =
      new base::PlatformFileError(base::PLATFORM_FILE_ERROR_FAILED);
  BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
      base::Bind(&CreatePlatformFileOnIOPool,
                 local_path,
                 file_flags,
                 platform_file,
                 open_error),
      base::Bind(&OnPlatformFileCreated,
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
    base::PlatformFileError error,
    const FilePath& local_path,
    const std::string& unused_mime_type,
    gdata::GDataFileType file_type) {
  scoped_refptr<ShareableFileReference> file_ref;

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

void OnClose(const FilePath& local_path, base::PlatformFileError error_code) {
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
    const fileapi::FileSystemOperationInterface::StatusCallback& callback,
    base::PlatformFileError truncate_result,
    base::PlatformFileError close_result) {
  // Reports the first error.
  callback.Run(truncate_result == base::PLATFORM_FILE_OK ? close_result
                                                         : truncate_result);
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

GDataFileSystemProxy::~GDataFileSystemProxy() {
  // Should be deleted from the CrosMountPointProvider on IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void GDataFileSystemProxy::GetFileInfo(const GURL& file_url,
    const FileSystemOperationInterface::GetMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
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

void GDataFileSystemProxy::Copy(const GURL& src_file_url,
    const GURL& dest_file_url,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Copy(src_file_path, dest_file_path, callback);
}

void GDataFileSystemProxy::Move(const GURL& src_file_url,
    const GURL& dest_file_url,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Move(src_file_path, dest_file_path, callback);
}

void GDataFileSystemProxy::ReadDirectory(const GURL& file_url,
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

void GDataFileSystemProxy::Remove(const GURL& file_url, bool recursive,
    const FileSystemOperationInterface::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Remove(file_path, recursive, callback);
}

void GDataFileSystemProxy::CreateDirectory(
    const GURL& file_url,
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

  file_system_->CreateDirectory(file_path, exclusive, recursive, callback);
}

void GDataFileSystemProxy::Truncate(const GURL& file_url, int64 length,
    const fileapi::FileSystemOperationInterface::StatusCallback& callback) {
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

void GDataFileSystemProxy::OnFileOpenedForTruncate(
    const FilePath& virtual_path,
    int64 length,
    const fileapi::FileSystemOperationInterface::StatusCallback& callback,
    base::PlatformFileError open_result,
    const FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (open_result != base::PLATFORM_FILE_OK) {
    callback.Run(open_result);
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
    const fileapi::FileSystemOperationInterface::StatusCallback& callback,
    base::PlatformFileError* truncate_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Truncation finished. We must close the file no matter |truncate_result|
  // indicates an error or not.
  file_system_->CloseFile(virtual_path, base::Bind(&DidCloseFileForTruncate,
                                                   callback,
                                                   *truncate_result));
}

void GDataFileSystemProxy::OpenFile(
    const GURL& file_url,
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

  file_system_->GetFileByPath(file_path,
                              base::Bind(&OnGetFileByPathForOpen,
                                         callback,
                                         file_flags,
                                         peer_handle),
                              GetDownloadDataCallback());
}

void GDataFileSystemProxy::CreateSnapshotFile(
    const GURL& file_url,
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
                 this, callback));
}

void GDataFileSystemProxy::OnGetEntryInfoByPath(
    const FileSystemOperationInterface::SnapshotFileCallback& callback,
    base::PlatformFileError error,
    const FilePath& entry_path,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != base::PLATFORM_FILE_OK || !entry_proto.get()) {
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
    const GURL& file_url,
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

// static.
bool GDataFileSystemProxy::ValidateUrl(const GURL& url, FilePath* file_path) {
  // what platform you're on.
  FilePath raw_path;
  fileapi::FileSystemType type = fileapi::kFileSystemTypeUnknown;
  if (!fileapi::CrackFileSystemURL(url, NULL, &type, file_path) ||
      type != fileapi::kFileSystemTypeExternal) {
    return false;
  }
  return true;
}

void GDataFileSystemProxy::OnGetMetadata(
    const FilePath& file_path,
    const FileSystemOperationInterface::GetMetadataCallback& callback,
    base::PlatformFileError error,
    const FilePath& entry_path,
    scoped_ptr<gdata::GDataEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, base::PlatformFileInfo(), FilePath());
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
    base::PlatformFileError error,
    bool hide_hosted_documents,
    scoped_ptr<gdata::GDataDirectoryProto> directory_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error, std::vector<base::FileUtilProxy::Entry>(), false);
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
    base::PlatformFileError result,
    const FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<ShareableFileReference> file_ref;

  if (result == base::PLATFORM_FILE_OK) {
    file_ref = ShareableFileReference::GetOrCreate(
        local_path,
        ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    file_ref->AddFinalReleaseCallback(
        base::Bind(&GDataFileSystemProxy::CloseWritableSnapshotFile,
                   this,
                   virtual_path));
  }

  callback.Run(result, local_path, file_ref);
}

void GDataFileSystemProxy::CloseWritableSnapshotFile(
    const FilePath& virtual_path,
    const FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  file_system_->CloseFile(virtual_path, base::Bind(&OnClose, virtual_path));
}

}  // namespace gdata
