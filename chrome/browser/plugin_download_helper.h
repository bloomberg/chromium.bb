// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_DOWNLOAD_HELPER_H_
#define CHROME_BROWSER_PLUGIN_DOWNLOAD_HELPER_H_
#pragma once

#include <string>
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "content/common/url_fetcher.h"
#include "net/base/file_stream.h"
#include "net/url_request/url_request.h"
#include "ui/gfx/native_widget_types.h"

// The PluginDownloadUrlHelper is used to handle one download URL request
// from the plugin. Each download request is handled by a new instance
// of this class.
class PluginDownloadUrlHelper : public URLFetcher::Delegate {
 public:
  // The delegate receives notification about the status of downloads
  // initiated.
  class DownloadDelegate {
   public:
    virtual ~DownloadDelegate() {}

    virtual void OnDownloadCompleted(const FilePath& download_path,
                                     bool success) {}
  };

  PluginDownloadUrlHelper(const std::string& download_url,
                          gfx::NativeWindow caller_window,
                          PluginDownloadUrlHelper::DownloadDelegate* delegate);
  ~PluginDownloadUrlHelper();

  void InitiateDownload(net::URLRequestContextGetter* request_context,
                        base::MessageLoopProxy* file_thread_proxy);

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const net::ResponseCookies& cookies,
                                  const std::string& data) {}
  virtual void OnURLFetchComplete(const URLFetcher* source);

  void OnDownloadCompleted(net::URLRequest* request);

 protected:
  // The download file request initiated by the plugin.
  scoped_ptr<URLFetcher> download_file_fetcher_;
  // TODO(port): this comment doesn't describe the situation on Posix.
  // The window handle for sending the WM_COPYDATA notification,
  // indicating that the download completed.
  gfx::NativeWindow download_file_caller_window_;

  std::string download_url_;

  PluginDownloadUrlHelper::DownloadDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PluginDownloadUrlHelper);
};

#endif  // OS_WIN

#endif  // CHROME_BROWSER_PLUGIN_DOWNLOAD_HELPER_H_


