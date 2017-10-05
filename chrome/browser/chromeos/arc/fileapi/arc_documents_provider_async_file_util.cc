// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_async_file_util.h"

#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/directory_entry.h"

using content::BrowserThread;

namespace arc {

namespace {

void OnGetFileInfoOnUIThread(
    const ArcDocumentsProviderRoot::GetFileInfoCallback& callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(callback, result, info));
}

void OnReadDirectoryOnUIThread(
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback,
    base::File::Error result,
    std::vector<ArcDocumentsProviderRoot::ThinFileInfo> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::AsyncFileUtil::EntryList entries;
  entries.reserve(files.size());
  for (const auto& file : files)
    entries.emplace_back(file.name, file.is_directory
                                        ? storage::DirectoryEntry::DIRECTORY
                                        : storage::DirectoryEntry::FILE);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(callback, result, entries, false /* has_more */));
}

void GetFileInfoOnUIThread(
    const storage::FileSystemURL& url,
    int fields,
    const ArcDocumentsProviderRoot::GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnGetFileInfoOnUIThread(callback, base::File::FILE_ERROR_SECURITY,
                            base::File::Info());
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnGetFileInfoOnUIThread(callback, base::File::FILE_ERROR_NOT_FOUND,
                            base::File::Info());
    return;
  }

  root->GetFileInfo(path, base::Bind(&OnGetFileInfoOnUIThread, callback));
}

void ReadDirectoryOnUIThread(
    const storage::FileSystemURL& url,
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnReadDirectoryOnUIThread(callback, base::File::FILE_ERROR_SECURITY, {});
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnReadDirectoryOnUIThread(callback, base::File::FILE_ERROR_NOT_FOUND, {});
    return;
  }

  root->ReadDirectory(path,
                      base::BindOnce(&OnReadDirectoryOnUIThread, callback));
}

}  // namespace

ArcDocumentsProviderAsyncFileUtil::ArcDocumentsProviderAsyncFileUtil() =
    default;

ArcDocumentsProviderAsyncFileUtil::~ArcDocumentsProviderAsyncFileUtil() =
    default;

void ArcDocumentsProviderAsyncFileUtil::CreateOrOpen(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(nya): Implement this function if it is ever called.
  NOTIMPLEMENTED();
  callback.Run(base::File(base::File::FILE_ERROR_INVALID_OPERATION),
               base::Closure());
}

void ArcDocumentsProviderAsyncFileUtil::EnsureFileExists(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED, false);
}

void ArcDocumentsProviderAsyncFileUtil::CreateDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::GetFileInfo(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int fields,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&GetFileInfoOnUIThread, url, fields, callback));
}

void ArcDocumentsProviderAsyncFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ReadDirectoryOnUIThread, url, callback));
}

void ArcDocumentsProviderAsyncFileUtil::Touch(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::Truncate(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64_t length,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::MoveFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::CopyInForeignFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::DeleteFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::DeleteDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::CreateSnapshotFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();  // TODO(crbug.com/671511): Implement this function.
  callback.Run(base::File::FILE_ERROR_FAILED, base::File::Info(),
               base::FilePath(),
               scoped_refptr<storage::ShareableFileReference>());
}

}  // namespace arc
