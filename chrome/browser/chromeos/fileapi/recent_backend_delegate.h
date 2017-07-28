// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_BACKEND_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend_delegate.h"
#include "chrome/browser/chromeos/fileapi/recent_async_file_util.h"

namespace chromeos {

// Implements recent file system.
class RecentBackendDelegate : public FileSystemBackendDelegate {
 public:
  RecentBackendDelegate();
  ~RecentBackendDelegate() override;

  // FileSystemBackend::Delegate overrides.
  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override;
  std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) override;
  std::unique_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset,
      storage::FileSystemContext* context) override;
  storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) override;
  void GetRedirectURLForContents(const storage::FileSystemURL& url,
                                 const storage::URLCallback& callback) override;

 private:
  RecentAsyncFileUtil async_file_util_;

  DISALLOW_COPY_AND_ASSIGN(RecentBackendDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_BACKEND_DELEGATE_H_
