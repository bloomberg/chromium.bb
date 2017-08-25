// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOAD_HANDLER
#define CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOAD_HANDLER

#include "content/browser/byte_stream.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/common/url_loader.mojom.h"

namespace content {

struct DownloadCreateInfo;

// Class for handling the download of a url. Implemented by child classes.
class CONTENT_EXPORT UrlDownloadHandler {
 public:
  // InputStream to read after the download starts. Only one of them could be
  // available at the same time.
  struct CONTENT_EXPORT InputStream {
    explicit InputStream(std::unique_ptr<ByteStreamReader> stream_reader);
    explicit InputStream(mojo::ScopedDataPipeConsumerHandle body);
    ~InputStream();

    std::unique_ptr<ByteStreamReader> stream_reader_;
    mojo::ScopedDataPipeConsumerHandle body_;
  };

  // Class to be notified when download starts/stops.
  class CONTENT_EXPORT Delegate {
   public:
    virtual void OnUrlDownloadStarted(
        std::unique_ptr<DownloadCreateInfo> download_create_info,
        std::unique_ptr<InputStream> input_stream,
        const DownloadUrlParameters::OnStartedCallback& callback) = 0;
    // Called after the connection is cancelled or finished.
    virtual void OnUrlDownloadStopped(UrlDownloadHandler* downloader) = 0;
  };

  UrlDownloadHandler() = default;
  virtual ~UrlDownloadHandler() = default;

  DISALLOW_COPY_AND_ASSIGN(UrlDownloadHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOAD_HANDLER
