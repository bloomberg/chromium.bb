// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_backend_delegate.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_stream_writer.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {

RecentBackendDelegate::RecentBackendDelegate() = default;

RecentBackendDelegate::~RecentBackendDelegate() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

storage::AsyncFileUtil* RecentBackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return &async_file_util_;
}

std::unique_ptr<storage::FileStreamReader>
RecentBackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();  // TODO(nya): Implement this function.
  return nullptr;
}

std::unique_ptr<storage::FileStreamWriter>
RecentBackendDelegate::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Read-only file system.
  return nullptr;
}

storage::WatcherManager* RecentBackendDelegate::GetWatcherManager(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Never called by chromeos::FileSystemBackend.
  return nullptr;
}

void RecentBackendDelegate::GetRedirectURLForContents(
    const storage::FileSystemURL& url,
    const storage::URLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED();  // Never called by chromeos::FileSystemBackend.
  callback.Run(GURL());
}

}  // namespace chromeos
