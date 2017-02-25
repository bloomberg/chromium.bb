// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_FILE_DOWNLOADER_H_
#define CHROME_BROWSER_NET_FILE_DOWNLOADER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

class GURL;

// Helper class to download a file from a given URL and store it in a local
// file. If |overwrite| is true, any existing file will be overwritten;
// otherwise if the local file already exists, this will report success without
// downloading anything.
class FileDownloader : public net::URLFetcherDelegate {
 public:
  enum Result {
    // The file was successfully downloaded.
    DOWNLOADED,
    // A local file at the given path already existed and was kept.
    EXISTS,
    // Downloading failed.
    FAILED
  };
  using DownloadFinishedCallback = base::Callback<void(Result)>;

  // Directly starts the download (if necessary) and runs |callback| when done.
  // If the instance is destroyed before it is finished, |callback| is not run.
  FileDownloader(const GURL& url,
                 const base::FilePath& path,
                 bool overwrite,
                 net::URLRequestContextGetter* request_context,
                 const DownloadFinishedCallback& callback,
                 const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~FileDownloader() override;

  static bool IsSuccess(Result result) { return result != FAILED; }

 private:
  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void OnFileExistsCheckDone(bool exists);

  void OnFileMoveDone(bool success);

  DownloadFinishedCallback callback_;

  std::unique_ptr<net::URLFetcher> fetcher_;

  base::FilePath local_path_;

  base::WeakPtrFactory<FileDownloader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileDownloader);
};

#endif  // CHROME_BROWSER_NET_FILE_DOWNLOADER_H_
