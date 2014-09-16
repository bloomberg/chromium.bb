// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/backend_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/buffering_file_stream_reader.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/file_stream_reader.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/file_stream_writer.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/file_stream_reader.h"
#include "storage/browser/fileapi/file_stream_writer.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {
namespace {

// Size of the stream reader internal buffer. At most this number of bytes will
// be read ahead of the requested data.
const int kReaderBufferSize = 512 * 1024;  // 512KB.

}  // namespace

BackendDelegate::BackendDelegate()
    : async_file_util_(new internal::ProviderAsyncFileUtil) {}

BackendDelegate::~BackendDelegate() {}

storage::AsyncFileUtil* BackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeProvided, type);
  return async_file_util_.get();
}

scoped_ptr<storage::FileStreamReader> BackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64 offset,
    int64 max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeProvided, url.type());

  return scoped_ptr<storage::FileStreamReader>(new BufferingFileStreamReader(
      scoped_ptr<storage::FileStreamReader>(new FileStreamReader(
          context, url, offset, expected_modification_time)),
      kReaderBufferSize,
      max_bytes_to_read));
}

scoped_ptr<storage::FileStreamWriter> BackendDelegate::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64 offset,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeProvided, url.type());

  return scoped_ptr<storage::FileStreamWriter>(
      new FileStreamWriter(url, offset));
}

storage::WatcherManager* BackendDelegate::GetWatcherManager(
    const storage::FileSystemURL& url) {
  NOTIMPLEMENTED();
  return NULL;
}

void BackendDelegate::GetRedirectURLForContents(
    const storage::FileSystemURL& url,
    const storage::URLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeProvided, url.type());
  callback.Run(GURL());
}

}  // namespace file_system_provider
}  // namespace chromeos
