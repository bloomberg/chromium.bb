// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DOWNLOAD_MANAGER_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DOWNLOAD_MANAGER_DELEGATE_H_

#include "content/public/browser/download_manager_delegate.h"

namespace android_webview {

// Android WebView does not use Chromium downloads, so implement methods here to
// unconditionally cancel the download.
class AwDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  virtual ~AwDownloadManagerDelegate();

  // content::DownloadManagerDelegate implementation.
  virtual bool DetermineDownloadTarget(
      content::DownloadItem* item,
      const content::DownloadTargetCallback& callback) OVERRIDE;
  virtual bool ShouldCompleteDownload(
      content::DownloadItem* item,
      const base::Closure& complete_callback) OVERRIDE;
  virtual bool ShouldOpenDownload(
      content::DownloadItem* item,
      const content::DownloadOpenDelayedCallback& callback) OVERRIDE;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DOWNLOAD_MANAGER_DELEGATE_H_
