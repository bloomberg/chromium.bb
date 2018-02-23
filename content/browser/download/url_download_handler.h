// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOAD_HANDLER
#define CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOAD_HANDLER

#include "components/download/public/common/download_url_parameters.h"
#include "content/public/browser/download_manager.h"

namespace download {
struct DownloadCreateInfo;
}  // namespace download

namespace content {

// Class for handling the download of a url. Implemented by child classes.
class CONTENT_EXPORT UrlDownloadHandler {
 public:
  // Class to be notified when download starts/stops.
  class CONTENT_EXPORT Delegate {
   public:
    virtual void OnUrlDownloadStarted(
        std::unique_ptr<download::DownloadCreateInfo> download_create_info,
        std::unique_ptr<DownloadManager::InputStream> input_stream,
        const download::DownloadUrlParameters::OnStartedCallback& callback) = 0;
    // Called after the connection is cancelled or finished.
    virtual void OnUrlDownloadStopped(UrlDownloadHandler* downloader) = 0;
  };

  UrlDownloadHandler() = default;
  virtual ~UrlDownloadHandler() = default;

  DISALLOW_COPY_AND_ASSIGN(UrlDownloadHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOAD_HANDLER
