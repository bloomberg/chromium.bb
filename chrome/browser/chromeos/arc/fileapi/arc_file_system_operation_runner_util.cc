// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner_util.h"

#include <utility>
#include <vector>

#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace file_system_operation_runner_util {

namespace {

// TODO(crbug.com/745648): Use correct BrowserContext.
ArcFileSystemOperationRunner* GetArcFileSystemOperationRunner() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ArcFileSystemOperationRunner::GetForBrowserContext(
      ArcServiceManager::Get()->browser_context());
}

template <typename T>
void PostToIOThread(const base::Callback<void(T)>& callback, T result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(callback, base::Passed(std::move(result))));
}

void GetFileSizeOnUIThread(const GURL& url,
                           const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner = GetArcFileSystemOperationRunner();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    callback.Run(-1);
    return;
  }
  runner->GetFileSize(url, callback);
}

void OpenFileToReadOnUIThread(const GURL& url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* runner = GetArcFileSystemOperationRunner();
  if (!runner) {
    DLOG(ERROR) << "ArcFileSystemOperationRunner unavailable. "
                << "File system operations are dropped.";
    callback.Run(mojo::ScopedHandle());
    return;
  }
  runner->OpenFileToRead(url, callback);
}

}  // namespace

void GetFileSizeOnIOThread(const GURL& url,
                           const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&GetFileSizeOnUIThread, url,
                     base::Bind(&PostToIOThread<int64_t>, callback)));
}

void OpenFileToReadOnIOThread(const GURL& url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &OpenFileToReadOnUIThread, url,
          base::Bind(&PostToIOThread<mojo::ScopedHandle>, callback)));
}

}  // namespace file_system_operation_runner_util

}  // namespace arc
