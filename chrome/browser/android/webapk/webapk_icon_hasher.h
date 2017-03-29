// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

// Downloads an icon and takes a Murmur2 hash of the downloaded image.
class WebApkIconHasher : public net::URLFetcherDelegate {
 public:
  using Murmur2HashCallback =
      base::Callback<void(const std::string& /* icon_murmur2_hash */)>;

  WebApkIconHasher(
      net::URLRequestContextGetter* request_context_getter,
      const GURL& icon_url,
      const Murmur2HashCallback& callback);
  ~WebApkIconHasher() override;

  // Downloads |icon_url_|. Calls |callback_| with the Murmur2 hash of the
  // downloaded image. The hash is taken over the raw image bytes (no image
  // encoding/decoding beforehand). |callback_| is called with an empty string
  // if the image cannot not be downloaded in time (e.g. 404 HTTP error code).
  void DownloadAndComputeMurmur2Hash();

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Called if downloading the icon takes too long.
  void OnDownloadTimedOut();

  // For retrieving URLRequestContext. Owned by the caller of this class.
  net::URLRequestContextGetter* url_request_context_getter_;

  // The icon URL.
  GURL icon_url_;

  // Called with the image hash.
  Murmur2HashCallback callback_;

  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Fails WebApkIconHasher if the download takes too long.
  base::OneShotTimer download_timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(WebApkIconHasher);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_
