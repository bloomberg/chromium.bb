// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_DOWNLOAD_HELPER_H_
#define CHROME_BROWSER_PLUGIN_DOWNLOAD_HELPER_H_
#pragma once

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class URLRequestContextGetter;
}

// The PluginDownloadUrlHelper is used to handle one download URL request
// from the plugin. Each download request is handled by a new instance
// of this class.
class PluginDownloadUrlHelper : public content::URLFetcherDelegate {
 public:
  typedef base::Callback<void(const FilePath&)> DownloadFinishedCallback;

  PluginDownloadUrlHelper();
  virtual ~PluginDownloadUrlHelper();

  void InitiateDownload(const GURL& download_url,
                        net::URLRequestContextGetter* request_context,
                        const DownloadFinishedCallback& callback);

  // content::URLFetcherDelegate
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

 private:
  // Renames the file (which was downloaded to a temporary file) to the filename
  // of the download URL.
  void RenameDownloadedFile();

  // Runs the callback and deletes itself.
  void RunCallback();

  // The download file request initiated by the plugin.
  scoped_ptr<content::URLFetcher> download_file_fetcher_;

  GURL download_url_;
  FilePath downloaded_file_;

  DownloadFinishedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PluginDownloadUrlHelper);
};

#endif  // CHROME_BROWSER_PLUGIN_DOWNLOAD_HELPER_H_
