// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"

#include <vector>

#include "base/debug/trace_event.h"
#include "base/files/file.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager.h"
#include "chrome/browser/chromeos/file_system_provider/operations/abort.h"
#include "chrome/browser/chromeos/file_system_provider/operations/close_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/copy_entry.h"
#include "chrome/browser/chromeos/file_system_provider/operations/create_directory.h"
#include "chrome/browser/chromeos/file_system_provider/operations/create_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/delete_entry.h"
#include "chrome/browser/chromeos/file_system_provider/operations/get_metadata.h"
#include "chrome/browser/chromeos/file_system_provider/operations/move_entry.h"
#include "chrome/browser/chromeos/file_system_provider/operations/open_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/read_directory.h"
#include "chrome/browser/chromeos/file_system_provider/operations/read_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/truncate.h"
#include "chrome/browser/chromeos/file_system_provider/operations/unmount.h"
#include "chrome/browser/chromeos/file_system_provider/operations/write_file.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "extensions/browser/event_router.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace chromeos {
namespace file_system_provider {
namespace {

// Dicards the result of Abort() when called from the destructor.
void EmptyStatusCallback(base::File::Error /* result */) {
}

}  // namespace

ProvidedFileSystem::ProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info)
    : profile_(profile),
      event_router_(extensions::EventRouter::Get(profile)),  // May be NULL.
      file_system_info_(file_system_info),
      notification_manager_(
          new NotificationManager(profile_, file_system_info_)),
      request_manager_(notification_manager_.get()),
      weak_ptr_factory_(this) {
}

ProvidedFileSystem::~ProvidedFileSystem() {
  const std::vector<int> request_ids = request_manager_.GetActiveRequestIds();
  for (size_t i = 0; i < request_ids.size(); ++i) {
    Abort(request_ids[i], base::Bind(&EmptyStatusCallback));
  }
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::RequestUnmount(
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      REQUEST_UNMOUNT,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::Unmount(event_router_, file_system_info_, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    MetadataFieldMask fields,
    const GetMetadataCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      GET_METADATA,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::GetMetadata(
          event_router_, file_system_info_, entry_path, fields, callback)));
  if (!request_id) {
    callback.Run(make_scoped_ptr<EntryMetadata>(NULL),
                 base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      READ_DIRECTORY,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::ReadDirectory(
              event_router_, file_system_info_, directory_path, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY,
                 storage::AsyncFileUtil::EntryList(),
                 false /* has_more */);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::ReadFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64 offset,
    int length,
    const ReadChunkReceivedCallback& callback) {
  TRACE_EVENT1(
      "file_system_provider", "ProvidedFileSystem::ReadFile", "length", length);
  const int request_id = request_manager_.CreateRequest(
      READ_FILE,
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new operations::ReadFile(event_router_,
                                   file_system_info_,
                                   file_handle,
                                   buffer,
                                   offset,
                                   length,
                                   callback)));
  if (!request_id) {
    callback.Run(0 /* chunk_length */,
                 false /* has_more */,
                 base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::OpenFile(
    const base::FilePath& file_path,
    OpenFileMode mode,
    const OpenFileCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      OPEN_FILE,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::OpenFile(
          event_router_, file_system_info_, file_path, mode, callback)));
  if (!request_id) {
    callback.Run(0 /* file_handle */, base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::CloseFile(
    int file_handle,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      CLOSE_FILE,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::CloseFile(
          event_router_, file_system_info_, file_handle, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      CREATE_DIRECTORY,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::CreateDirectory(event_router_,
                                          file_system_info_,
                                          directory_path,
                                          recursive,
                                          callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      DELETE_ENTRY,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::DeleteEntry(
          event_router_, file_system_info_, entry_path, recursive, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::CreateFile(
    const base::FilePath& file_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      CREATE_FILE,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::CreateFile(
          event_router_, file_system_info_, file_path, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      COPY_ENTRY,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::CopyEntry(event_router_,
                                    file_system_info_,
                                    source_path,
                                    target_path,
                                    callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64 offset,
    int length,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  TRACE_EVENT1("file_system_provider",
               "ProvidedFileSystem::WriteFile",
               "length",
               length);
  const int request_id = request_manager_.CreateRequest(
      WRITE_FILE,
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new operations::WriteFile(event_router_,
                                    file_system_info_,
                                    file_handle,
                                    make_scoped_refptr(buffer),
                                    offset,
                                    length,
                                    callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      MOVE_ENTRY,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::MoveEntry(event_router_,
                                    file_system_info_,
                                    source_path,
                                    target_path,
                                    callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::Truncate(
    const base::FilePath& file_path,
    int64 length,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_.CreateRequest(
      TRUNCATE,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::Truncate(
          event_router_, file_system_info_, file_path, length, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

const ProvidedFileSystemInfo& ProvidedFileSystem::GetFileSystemInfo() const {
  return file_system_info_;
}

RequestManager* ProvidedFileSystem::GetRequestManager() {
  return &request_manager_;
}

base::WeakPtr<ProvidedFileSystemInterface> ProvidedFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProvidedFileSystem::Abort(
    int operation_request_id,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  request_manager_.RejectRequest(operation_request_id,
                                 make_scoped_ptr(new RequestValue()),
                                 base::File::FILE_ERROR_ABORT);
  if (!request_manager_.CreateRequest(
          ABORT,
          scoped_ptr<RequestManager::HandlerInterface>(
              new operations::Abort(event_router_,
                                    file_system_info_,
                                    operation_request_id,
                                    callback)))) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
  }
}

}  // namespace file_system_provider
}  // namespace chromeos
