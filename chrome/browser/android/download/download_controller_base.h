// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_CONTROLLER_BASE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_CONTROLLER_BASE_H_

#include <string>

#include "base/callback.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/context_menu_params.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {
class DownloadItem;
class WebContents;
}

// Used to store all the information about an Android download.
struct DownloadInfo {
  explicit DownloadInfo(const net::URLRequest* request);
  DownloadInfo(const DownloadInfo& other);
  ~DownloadInfo();

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |original_url_|.
  GURL url;
  // The original URL before any redirection by the server for this URL.
  GURL original_url;
  std::string content_disposition;
  std::string original_mime_type;
  std::string user_agent;
  std::string cookie;
  std::string referer;
};

// Interface to request GET downloads and send notifications for POST
// downloads.
class DownloadControllerBase : public content::DownloadItem::Observer {
 public:
  // Returns the singleton instance of the DownloadControllerBase.
  static DownloadControllerBase* Get();

  // Called to set the DownloadControllerBase instance.
  static void SetDownloadControllerBase(
      DownloadControllerBase* download_controller);

  // Should be called when a download is started. It can be either a GET
  // request with authentication or a POST request. Notifies the embedding
  // app about the download. Should be called on the UI thread.
  virtual void OnDownloadStarted(content::DownloadItem* download_item) = 0;

  // Called when a download is initiated by context menu.
  virtual void StartContextMenuDownload(
      const content::ContextMenuParams& params,
      content::WebContents* web_contents,
      bool is_link, const std::string& extra_headers) = 0;

  // Callback when user permission prompt finishes. Args: whether file access
  // permission is acquired.
  typedef base::Callback<void(bool)> AcquireFileAccessPermissionCallback;

  // Called to prompt the user for file access permission. When finished,
  // |callback| will be executed.
  virtual void AcquireFileAccessPermission(
      content::WebContents* web_contents,
      const AcquireFileAccessPermissionCallback& callback) = 0;

  // Called by unit test to approve or disapprove file access request.
  virtual void SetApproveFileAccessRequestForTesting(bool approve) {}

  // Starts a new download request with Android DownloadManager.
  virtual void CreateAndroidDownload(
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const DownloadInfo& info) = 0;

 protected:
  ~DownloadControllerBase() override {}
  static DownloadControllerBase* download_controller_;
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_CONTROLLER_BASE_H_
