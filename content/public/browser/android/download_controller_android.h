// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_

#include "content/common/content_export.h"

namespace content {
class DownloadItem;
class RenderViewHost;
class WebContents;

// Interface to request GET downloads and send notifications for POST
// downloads.
class CONTENT_EXPORT DownloadControllerAndroid {
 public:
  // Returns the singleton instance of the DownloadControllerAndroid.
  static DownloadControllerAndroid* Get();

  // Starts a new download request with Android. Should be called on the
  // UI thread.
  virtual void CreateGETDownload(RenderViewHost* source, int request_id) = 0;

  // Should be called when a POST download is started. Notifies the embedding
  // app about the download. Should be called on the UI thread.
  virtual void OnPostDownloadStarted(WebContents* web_contents,
                                     DownloadItem* download_item) = 0;
 protected:
  virtual ~DownloadControllerAndroid() {};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_H_
