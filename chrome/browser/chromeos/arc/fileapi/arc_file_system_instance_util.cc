// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_instance_util.h"

#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace arc {

namespace file_system_instance_util {

namespace {

constexpr uint32_t kGetFileSizeVersion = 1;
constexpr uint32_t kOpenFileToReadVersion = 1;

void OnGetFileSize(const GetFileSizeCallback& callback, int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                   base::Bind(callback, size));
}

void GetFileSizeOnUIThread(const GURL& arc_url,
                           const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* arc_bridge_service = arc::ArcBridgeService::Get();
  if (!arc_bridge_service) {
    LOG(ERROR) << "Failed to get ArcBridgeService.";
    OnGetFileSize(callback, -1);
    return;
  }
  mojom::FileSystemInstance* file_system_instance =
      arc_bridge_service->file_system()->GetInstanceForMethod(
          "GetFileSize", kGetFileSizeVersion);
  if (!file_system_instance) {
    LOG(ERROR) << "Failed to get FileSystemInstance.";
    OnGetFileSize(callback, -1);
    return;
  }
  file_system_instance->GetFileSize(arc_url.spec(),
                                    base::Bind(&OnGetFileSize, callback));
}

void OnOpenFileToRead(const OpenFileToReadCallback& callback,
                      mojo::ScopedHandle handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                   base::Bind(callback, base::Passed(&handle)));
}

void OpenFileToReadOnUIThread(const GURL& arc_url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* arc_bridge_service = arc::ArcBridgeService::Get();
  if (!arc_bridge_service) {
    LOG(ERROR) << "Failed to get ArcBridgeService.";
    OnOpenFileToRead(callback, mojo::ScopedHandle());
    return;
  }
  mojom::FileSystemInstance* file_system_instance =
      arc_bridge_service->file_system()->GetInstanceForMethod(
          "OpenFileToRead", kOpenFileToReadVersion);
  if (!file_system_instance) {
    LOG(ERROR) << "Failed to get FileSystemInstance.";
    OnOpenFileToRead(callback, mojo::ScopedHandle());
    return;
  }
  file_system_instance->OpenFileToRead(arc_url.spec(),
                                       base::Bind(&OnOpenFileToRead, callback));
}

}  // namespace

void GetFileSizeOnIOThread(const GURL& arc_url,
                           const GetFileSizeCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&GetFileSizeOnUIThread, arc_url, callback));
}

void OpenFileToReadOnIOThread(const GURL& arc_url,
                              const OpenFileToReadCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&OpenFileToReadOnUIThread, arc_url, callback));
}

}  // namespace file_system_instance_util

}  // namespace arc
