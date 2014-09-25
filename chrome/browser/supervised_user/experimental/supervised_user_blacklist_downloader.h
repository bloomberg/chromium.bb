// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_BLACKLIST_DOWNLOADER_H_
#define CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_BLACKLIST_DOWNLOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class FilePath;
}  // namespace base

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

class GURL;

// Helper class to download a blacklist file from a given URL and store it in a
// local file. If the local file already exists, reports success without
// downloading anything.
class SupervisedUserBlacklistDownloader : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(bool /* success */)> DownloadFinishedCallback;

  // Directly starts the download (if necessary) and runs |callback| when done.
  // If the instance is destroyed before it is finished, |callback| is not run.
  SupervisedUserBlacklistDownloader(
      const GURL& url,
      const base::FilePath& path,
      net::URLRequestContextGetter* request_context,
      const DownloadFinishedCallback& callback);
  virtual ~SupervisedUserBlacklistDownloader();

 private:
  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  void OnFileExistsCheckDone(bool exists);

  DownloadFinishedCallback callback_;

  scoped_ptr<net::URLFetcher> fetcher_;

  base::WeakPtrFactory<SupervisedUserBlacklistDownloader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserBlacklistDownloader);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SUPERVISED_USER_BLACKLIST_DOWNLOADER_H_
