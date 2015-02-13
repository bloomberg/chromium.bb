// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/deferred_download_observer.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/android/download_controller_android_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {

DeferredDownloadObserver::DeferredDownloadObserver(
    WebContents* web_contents,
    const base::Closure& deferred_download_cb)
    : WebContentsObserver(web_contents),
      deferred_download_cb_(deferred_download_cb) {
}

DeferredDownloadObserver::~DeferredDownloadObserver() {}

void DeferredDownloadObserver::WasShown() {
  deferred_download_cb_.Run();
  DownloadControllerAndroidImpl::GetInstance()->CancelDeferredDownload(this);
}

void DeferredDownloadObserver::WebContentsDestroyed() {
  DownloadControllerAndroidImpl::GetInstance()->CancelDeferredDownload(this);
}

}  // namespace content
