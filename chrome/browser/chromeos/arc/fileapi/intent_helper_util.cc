// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/intent_helper_util.h"

#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace intent_helper_util {

namespace {

constexpr uint32_t kGetFileSizeVersion = 15;
constexpr uint32_t kOpenFileToReadVersion = 15;

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
  mojom::IntentHelperInstance* intent_helper_instance =
      arc_bridge_service->intent_helper()->GetInstanceForMethod(
          "GetFileSizeDeprecated", kGetFileSizeVersion);
  if (!intent_helper_instance) {
    LOG(ERROR) << "Failed to get IntentHelperInstance.";
    OnGetFileSize(callback, -1);
    return;
  }
  intent_helper_instance->GetFileSizeDeprecated(
      arc_url.spec(), base::Bind(&OnGetFileSize, callback));
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
  mojom::IntentHelperInstance* intent_helper_instance =
      arc_bridge_service->intent_helper()->GetInstanceForMethod(
          "OpenFileToReadDeprecated", kOpenFileToReadVersion);
  if (!intent_helper_instance) {
    LOG(ERROR) << "Failed to get IntentHelperInstance.";
    OnOpenFileToRead(callback, mojo::ScopedHandle());
    return;
  }
  intent_helper_instance->OpenFileToReadDeprecated(
      arc_url.spec(), base::Bind(&OnOpenFileToRead, callback));
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

}  // namespace intent_helper_util

}  // namespace arc
