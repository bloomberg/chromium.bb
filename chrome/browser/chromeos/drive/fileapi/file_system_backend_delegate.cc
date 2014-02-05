// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi/file_system_backend_delegate.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/fileapi/async_file_util.h"
#include "chrome/browser/chromeos/drive/fileapi/fileapi_worker.h"
#include "chrome/browser/chromeos/drive/fileapi/webkit_file_stream_reader_impl.h"
#include "chrome/browser/chromeos/drive/fileapi/webkit_file_stream_writer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/async_file_util.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace drive {

FileSystemBackendDelegate::FileSystemBackendDelegate()
    : async_file_util_(new internal::AsyncFileUtil) {
}

FileSystemBackendDelegate::~FileSystemBackendDelegate() {
}

fileapi::AsyncFileUtil* FileSystemBackendDelegate::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeDrive, type);
  return async_file_util_.get();
}

scoped_ptr<webkit_blob::FileStreamReader>
FileSystemBackendDelegate::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeDrive, url.type());

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  if (file_path.empty())
    return scoped_ptr<webkit_blob::FileStreamReader>();

  return scoped_ptr<webkit_blob::FileStreamReader>(
      new internal::WebkitFileStreamReaderImpl(
          base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
          context->default_file_task_runner(),
          file_path, offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
FileSystemBackendDelegate::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(fileapi::kFileSystemTypeDrive, url.type());

  base::FilePath file_path = util::ExtractDrivePathFromFileSystemUrl(url);
  // Hosted documents don't support stream writer.
  if (file_path.empty() || util::HasGDocFileExtension(file_path))
    return scoped_ptr<fileapi::FileStreamWriter>();

  return scoped_ptr<fileapi::FileStreamWriter>(
      new internal::WebkitFileStreamWriterImpl(
          base::Bind(&fileapi_internal::GetFileSystemFromUrl, url),
          context->default_file_task_runner(),file_path, offset));
}

}  // namespace drive
