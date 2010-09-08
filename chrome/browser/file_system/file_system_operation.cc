// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_operation.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/file_system/file_system_operation_client.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileError.h"

namespace {
// Utility method for error conversions.
WebKit::WebFileError PlatformToWebkitError(base::PlatformFileError rv) {
  switch (rv) {
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
        return WebKit::WebFileErrorNotFound;
    case base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
    case base::PLATFORM_FILE_ERROR_EXISTS:
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
         return WebKit::WebFileErrorInvalidModification;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
        return WebKit::WebFileErrorNoModificationAllowed;
    default:
        return WebKit::WebFileErrorInvalidModification;
  }
}
}  // namespace

FileSystemOperation::FileSystemOperation(
    int request_id, FileSystemOperationClient* client)
    : client_(client),
      request_id_(request_id),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      operation_pending_(false) {
  DCHECK(client_);
}

void FileSystemOperation::CreateFile(const FilePath& path,
                                     bool exclusive) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::CreateOrOpen(
    ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
    path, base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_READ,
    callback_factory_.NewCallback(
        exclusive ? &FileSystemOperation::DidCreateFileExclusive
                  : &FileSystemOperation::DidCreateFileNonExclusive));
}

void FileSystemOperation::CreateDirectory(const FilePath& path,
                                          bool exclusive) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::CreateDirectory(
      ChromeThread::GetMessageLoopProxyForThread(
          ChromeThread::FILE), path, exclusive, false, /* recursive */
          callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Copy(const FilePath& src_path,
                               const FilePath& dest_path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::Copy(
      ChromeThread::GetMessageLoopProxyForThread(
          ChromeThread::FILE), src_path, dest_path,
          callback_factory_.NewCallback(
              &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Move(const FilePath& src_path,
                               const FilePath& dest_path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::Move(
      ChromeThread::GetMessageLoopProxyForThread(
          ChromeThread::FILE), src_path, dest_path,
          callback_factory_.NewCallback(
              &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::DirectoryExists(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::GetFileInfo(
      ChromeThread::GetMessageLoopProxyForThread(
          ChromeThread::FILE),
      path, callback_factory_.NewCallback(
              &FileSystemOperation::DidDirectoryExists));
}

void FileSystemOperation::FileExists(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::GetFileInfo(
      ChromeThread::GetMessageLoopProxyForThread(
          ChromeThread::FILE), path,
          callback_factory_.NewCallback(
              &FileSystemOperation::DidFileExists));
}

void FileSystemOperation::GetMetadata(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::GetFileInfo(
      ChromeThread::GetMessageLoopProxyForThread(
          ChromeThread::FILE), path,
          callback_factory_.NewCallback(
              &FileSystemOperation::DidGetMetadata));
}

void FileSystemOperation::ReadDirectory(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::ReadDirectory(
      ChromeThread::GetMessageLoopProxyForThread(
          ChromeThread::FILE), path,
          callback_factory_.NewCallback(
              &FileSystemOperation::DidReadDirectory));
}

void FileSystemOperation::Remove(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::Delete(
      ChromeThread::GetMessageLoopProxyForThread(
          ChromeThread::FILE), path,
          callback_factory_.NewCallback(
              &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::DidCreateFileExclusive(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  DidFinishFileOperation(rv);
}

void FileSystemOperation::DidCreateFileNonExclusive(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  // Supress the already exists error and report success.
  if (rv == base::PLATFORM_FILE_OK ||
      rv == base::PLATFORM_FILE_ERROR_EXISTS)
    client_->DidSucceed(request_id_);
  else
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
}

void FileSystemOperation::DidFinishFileOperation(
    base::PlatformFileError rv) {
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidSucceed(request_id_);
  else
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
}

void FileSystemOperation::DidDirectoryExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      client_->DidSucceed(request_id_);
    else
      client_->DidFail(WebKit::WebFileErrorInvalidState,
                       request_id_);
  } else {
    // Something else went wrong.
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
  }
}

void FileSystemOperation::DidFileExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      client_->DidFail(WebKit::WebFileErrorInvalidState,
                       request_id_);
    else
      client_->DidSucceed(request_id_);
  } else {
    // Something else went wrong.
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
  }
}

void FileSystemOperation::DidGetMetadata(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidReadMetadata(file_info, request_id_);
  else
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
}

void FileSystemOperation::DidReadDirectory(
    base::PlatformFileError rv,
    const std::vector<base::file_util_proxy::Entry>& entries) {
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidReadDirectory(
        entries, false /* has_more */ , request_id_);
  else
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
}
