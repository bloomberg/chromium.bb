// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_H_
#define CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/image_decoder.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

class SkBitmap;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace chrome {

// Asynchrounously fetches an image from the given URL and returns the
// decoded Bitmap to the provided BitmapFetcherDelegate.
class BitmapFetcher : public net::URLFetcherDelegate,
                      public ImageDecoder::ImageRequest {
 public:
  BitmapFetcher(const GURL& url,
                BitmapFetcherDelegate* delegate,
                const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~BitmapFetcher() override;

  const GURL& url() const { return url_; }
  net::URLFetcher* url_fetcher() { return url_fetcher_.get(); }

  // Initializes internal fetcher.  After this function returns url_fetcher()
  // can be accessed to configure it further (eg. add user data to request).
  // All configuration must be done before Start() is called.
  // Values for |load_flags| are defined in net/base/load_flags.h.  In general,
  // |net::LOAD_NORMAL| is appropriate.  Init may be called more than once in
  // some cases.  If so, subsequent starts will be ignored.
  void Init(net::URLRequestContextGetter* request_context,
            const std::string& referrer,
            net::URLRequest::ReferrerPolicy referrer_policy,
            int load_flags);

  // Start fetching the URL with the fetcher. The delegate is notified
  // asynchronously when done.  Start may be called more than once in some
  // cases.  If so, subsequent starts will be ignored since the operation is
  // already in progress.
  void Start();

  // Methods inherited from URLFetcherDelegate

  // This will be called when the URL has been fetched, successfully or not.
  // Use accessor methods on |source| to get the results.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Methods inherited from ImageDecoder::ImageRequest

  // Called when image is decoded. |decoder| is used to identify the image in
  // case of decoding several images simultaneously.  This will not be called
  // on the UI thread.
  void OnImageDecoded(const SkBitmap& decoded_image) override;

  // Called when decoding image failed.
  void OnDecodeImageFailed() override;

 private:
  // Alerts the delegate that a failure occurred.
  void ReportFailure();

  std::unique_ptr<net::URLFetcher> url_fetcher_;
  const GURL url_;
  BitmapFetcherDelegate* const delegate_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcher);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_H_
