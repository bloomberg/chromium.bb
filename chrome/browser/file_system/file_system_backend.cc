// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/file_system/file_system_backend.h"
#include "chrome/browser/file_system/file_system_backend_client.h"
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
        return WebKit::WebFileErrorInvalidModification;
    default:
        return WebKit::WebFileErrorNoModificationAllowed;
  }
}
}  // namespace

FileSystemBackend::FileSystemBackend()
    : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

void FileSystemBackend::set_client(FileSystemBackendClient* client) {
  client_ = client;
}

void FileSystemBackend::CreateFile(const FilePath& path,
                                   bool exclusive,
                                   int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::CreateOrOpen(
    ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
    path, base::PLATFORM_FILE_CREATE,
    callback_factory_.NewCallback(
        exclusive ? &FileSystemBackend::DidCreateFileExclusive
                  : &FileSystemBackend::DidCreateFileNonExclusive));
}

void FileSystemBackend::CreateDirectory(const FilePath& path,
                                        bool exclusive,
                                        int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::CreateDirectory(
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
      path, exclusive, callback_factory_.NewCallback(
          &FileSystemBackend::DidFinishFileOperation));
}

void FileSystemBackend::Copy(const FilePath& src_path,
                             const FilePath& dest_path,
                             int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::Copy(
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
      src_path, dest_path, callback_factory_.NewCallback(
          &FileSystemBackend::DidFinishFileOperation));
}

void FileSystemBackend::Move(const FilePath& src_path,
                             const FilePath& dest_path,
                             int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::Move(
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
      src_path, dest_path, callback_factory_.NewCallback(
          &FileSystemBackend::DidFinishFileOperation));
}

void FileSystemBackend::DirectoryExists(const FilePath& path, int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::GetFileInfo(
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
      path, callback_factory_.NewCallback(
              &FileSystemBackend::DidDirectoryExists));
}

void FileSystemBackend::FileExists(const FilePath& path, int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::GetFileInfo(
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
      path, callback_factory_.NewCallback(&FileSystemBackend::DidFileExists));
}

void FileSystemBackend::GetMetadata(const FilePath& path, int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::GetFileInfo(
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
      path, callback_factory_.NewCallback(&FileSystemBackend::DidGetMetadata));
}

void FileSystemBackend::ReadDirectory(
    const FilePath& path, int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::ReadDirectory(
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
      path, callback_factory_.NewCallback(
          &FileSystemBackend::DidReadDirectory));
}

void FileSystemBackend::Remove(const FilePath& path, int request_id) {
  request_id_ = request_id;
  base::FileUtilProxy::Delete(
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
      path, callback_factory_.NewCallback(
          &FileSystemBackend::DidFinishFileOperation));
}

void FileSystemBackend::DidCreateFileExclusive(base::PlatformFileError rv,
                                               base::PassPlatformFile file,
                                               bool created) {
  DidFinishFileOperation(rv);
}

void FileSystemBackend::DidCreateFileNonExclusive(base::PlatformFileError rv,
                                                  base::PassPlatformFile file,
                                                  bool created) {
  // Supress the already exists error and report success.
  if (rv == base::PLATFORM_FILE_OK ||
      rv == base::PLATFORM_FILE_ERROR_EXISTS)
    client_->DidSucceed(rv);
  else
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
}

void FileSystemBackend::DidFinishFileOperation(base::PlatformFileError rv) {
  DCHECK(client_);
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidSucceed(request_id_);
  else
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
}

void FileSystemBackend::DidDirectoryExists(
    base::PlatformFileError rv, const base::PlatformFileInfo& file_info) {
  DCHECK(client_);
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      client_->DidSucceed(request_id_);
    else
      client_->DidFail(WebKit::WebFileErrorInvalidState, request_id_);
  } else {
    // Something else went wrong.
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
  }
}

void FileSystemBackend::DidFileExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  DCHECK(client_);
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      client_->DidFail(WebKit::WebFileErrorInvalidState, request_id_);
    else
      client_->DidSucceed(request_id_);
  } else {
    // Something else went wrong.
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
  }
}

void FileSystemBackend::DidGetMetadata(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  DCHECK(client_);
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidReadMetadata(file_info, request_id_);
  else
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
}

void FileSystemBackend::DidReadDirectory(
    base::PlatformFileError rv,
    const std::vector<base::file_util_proxy::Entry>& entries) {
  DCHECK(client_);
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidReadDirectory(entries, false /* has_more */ , request_id_);
  else
    client_->DidFail(PlatformToWebkitError(rv), request_id_);
}
