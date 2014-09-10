// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/blob/shareable_file_reference.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {
namespace internal {
namespace {

// Executes GetFileInfo on the UI thread.
void GetFileInfoOnUIThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ProvidedFileSystemInterface::GetMetadataCallback& callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(make_scoped_ptr<EntryMetadata>(NULL),
                 base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->GetMetadata(
      parser.file_path(),
      ProvidedFileSystemInterface::METADATA_FIELD_DEFAULT,
      callback);
}

// Routes the response of GetFileInfo back to the IO thread with a type
// conversion.
void OnGetFileInfo(const storage::AsyncFileUtil::GetFileInfoCallback& callback,
                   scoped_ptr<EntryMetadata> metadata,
                   base::File::Error result) {
  if (result != base::File::FILE_OK) {
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(callback, result, base::File::Info()));
    return;
  }

  DCHECK(metadata.get());
  base::File::Info file_info;

  // TODO(mtomasz): Add support for last modified time and creation time.
  // See: crbug.com/388540.
  file_info.size = metadata->size;
  file_info.is_directory = metadata->is_directory;
  file_info.is_symbolic_link = false;  // Not supported.
  file_info.last_modified = metadata->modification_time;
  file_info.last_accessed = metadata->modification_time;  // Not supported.
  file_info.creation_time = metadata->modification_time;  // Not supported.

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(callback, base::File::FILE_OK, file_info));
}

// Executes ReadDirectory on the UI thread.
void ReadDirectoryOnUIThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION,
                 storage::AsyncFileUtil::EntryList(),
                 false /* has_more */);
    return;
  }

  parser.file_system()->ReadDirectory(parser.file_path(), callback);
}

// Routes the response of ReadDirectory back to the IO thread.
void OnReadDirectory(
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback,
    base::File::Error result,
    const storage::AsyncFileUtil::EntryList& entry_list,
    bool has_more) {
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(callback, result, entry_list, has_more));
}

// Executes CreateDirectory on the UI thread.
void CreateDirectoryOnUIThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->CreateDirectory(
      parser.file_path(), recursive, callback);
}

// Routes the response of CreateDirectory back to the IO thread.
void OnCreateDirectory(bool exclusive,
                       const storage::AsyncFileUtil::StatusCallback& callback,
                       base::File::Error result) {
  // If the directory already existed and the operation wasn't exclusive, then
  // return success anyway, since it is not an error.
  const base::File::Error error =
      (result == base::File::FILE_ERROR_EXISTS && !exclusive)
          ? base::File::FILE_OK
          : result;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, error));
}

// Executes DeleteEntry on the UI thread.
void DeleteEntryOnUIThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->DeleteEntry(parser.file_path(), recursive, callback);
}

// Routes the response of DeleteEntry back to the IO thread.
void OnDeleteEntry(const storage::AsyncFileUtil::StatusCallback& callback,
                   base::File::Error result) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
}

// Executes CreateFile on the UI thread.
void CreateFileOnUIThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->CreateFile(parser.file_path(), callback);
}

// Routes the response of CreateFile to a callback of EnsureFileExists() on the
// IO thread.
void OnCreateFileForEnsureFileExists(
    const storage::AsyncFileUtil::EnsureFileExistsCallback& callback,
    base::File::Error result) {
  const bool created = result == base::File::FILE_OK;

  // If the file already existed, then return success anyway, since it is not
  // an error.
  const base::File::Error error =
      result == base::File::FILE_ERROR_EXISTS ? base::File::FILE_OK : result;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, error, created));
}

// Executes CopyEntry on the UI thread.
void CopyEntryOnUIThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& target_url,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  util::FileSystemURLParser source_parser(source_url);
  util::FileSystemURLParser target_parser(target_url);

  if (!source_parser.Parse() || !target_parser.Parse() ||
      source_parser.file_system() != target_parser.file_system()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  target_parser.file_system()->CopyEntry(
      source_parser.file_path(), target_parser.file_path(), callback);
}

// Routes the response of CopyEntry to a callback of CopyLocalFile() on the
// IO thread.
void OnCopyEntry(const storage::AsyncFileUtil::StatusCallback& callback,
                 base::File::Error result) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
}

// Executes MoveEntry on the UI thread.
void MoveEntryOnUIThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& target_url,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  util::FileSystemURLParser source_parser(source_url);
  util::FileSystemURLParser target_parser(target_url);

  if (!source_parser.Parse() || !target_parser.Parse() ||
      source_parser.file_system() != target_parser.file_system()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  target_parser.file_system()->MoveEntry(
      source_parser.file_path(), target_parser.file_path(), callback);
}

// Routes the response of CopyEntry to a callback of MoveLocalFile() on the
// IO thread.
void OnMoveEntry(const storage::AsyncFileUtil::StatusCallback& callback,
                 base::File::Error result) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
}

// Executes Truncate on the UI thread.
void TruncateOnUIThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64 length,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->Truncate(parser.file_path(), length, callback);
}

// Routes the response of Truncate back to the IO thread.
void OnTruncate(const storage::AsyncFileUtil::StatusCallback& callback,
                base::File::Error result) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
}

}  // namespace

ProviderAsyncFileUtil::ProviderAsyncFileUtil() {}

ProviderAsyncFileUtil::~ProviderAsyncFileUtil() {}

void ProviderAsyncFileUtil::CreateOrOpen(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if ((file_flags & base::File::FLAG_CREATE) ||
      (file_flags & base::File::FLAG_OPEN_ALWAYS) ||
      (file_flags & base::File::FLAG_CREATE_ALWAYS) ||
      (file_flags & base::File::FLAG_OPEN_TRUNCATED)) {
    callback.Run(base::File(base::File::FILE_ERROR_ACCESS_DENIED),
                 base::Closure());
    return;
  }

  NOTIMPLEMENTED();
  callback.Run(base::File(base::File::FILE_ERROR_INVALID_OPERATION),
               base::Closure());
}

void ProviderAsyncFileUtil::EnsureFileExists(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CreateFileOnUIThread,
                 base::Passed(&context),
                 url,
                 base::Bind(&OnCreateFileForEnsureFileExists, callback)));
}

void ProviderAsyncFileUtil::CreateDirectory(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CreateDirectoryOnUIThread,
                 base::Passed(&context),
                 url,
                 exclusive,
                 recursive,
                 base::Bind(&OnCreateDirectory, exclusive, callback)));
}

void ProviderAsyncFileUtil::GetFileInfo(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&GetFileInfoOnUIThread,
                                     base::Passed(&context),
                                     url,
                                     base::Bind(&OnGetFileInfo, callback)));
}

void ProviderAsyncFileUtil::ReadDirectory(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ReadDirectoryOnUIThread,
                                     base::Passed(&context),
                                     url,
                                     base::Bind(&OnReadDirectory, callback)));
}

void ProviderAsyncFileUtil::Touch(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::Truncate(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&TruncateOnUIThread,
                                     base::Passed(&context),
                                     url,
                                     length,
                                     base::Bind(&OnTruncate, callback)));
}

void ProviderAsyncFileUtil::CopyFileLocal(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mtomasz): Consier adding support for options (preserving last modified
  // time) as well as the progress callback.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&CopyEntryOnUIThread,
                                     base::Passed(&context),
                                     src_url,
                                     dest_url,
                                     base::Bind(&OnCopyEntry, callback)));
}

void ProviderAsyncFileUtil::MoveFileLocal(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mtomasz): Consier adding support for options (preserving last modified
  // time) as well as the progress callback.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&MoveEntryOnUIThread,
                                     base::Passed(&context),
                                     src_url,
                                     dest_url,
                                     base::Bind(&OnMoveEntry, callback)));
}

void ProviderAsyncFileUtil::CopyInForeignFile(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::DeleteFile(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&DeleteEntryOnUIThread,
                                     base::Passed(&context),
                                     url,
                                     false,  // recursive
                                     base::Bind(&OnDeleteEntry, callback)));
}

void ProviderAsyncFileUtil::DeleteDirectory(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&DeleteEntryOnUIThread,
                                     base::Passed(&context),
                                     url,
                                     false,  // recursive
                                     base::Bind(&OnDeleteEntry, callback)));
}

void ProviderAsyncFileUtil::DeleteRecursively(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&DeleteEntryOnUIThread,
                                     base::Passed(&context),
                                     url,
                                     true,  // recursive
                                     base::Bind(&OnDeleteEntry, callback)));
}

void ProviderAsyncFileUtil::CreateSnapshotFile(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION,
               base::File::Info(),
               base::FilePath(),
               scoped_refptr<storage::ShareableFileReference>());
}

}  // namespace internal
}  // namespace file_system_provider
}  // namespace chromeos
