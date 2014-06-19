// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_H_
#define CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/bitmap_fetcher_delegate.h"
#include "chrome/browser/image_decoder.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace chrome {

// Asynchrounously fetches an image from the given URL and returns the
// decoded Bitmap to the provided BitmapFetcherDelegate.
class BitmapFetcher : public net::URLFetcherDelegate,
                      public ImageDecoder::Delegate {
 public:
  BitmapFetcher(const GURL& url, BitmapFetcherDelegate* delegate);
  virtual ~BitmapFetcher();

  const GURL& url() const { return url_; }

  // Start fetching the URL with the fetcher. The delegate is notified
  // asynchronously when done.  Start may be called more than once in some
  // cases.  If so, subsequent starts will be ignored since the operation is
  // already in progress.  Arguments are used to configure the internal fetcher.
  // Values for |load_flags| are defined in net/base/load_flags.h.  In general,
  // |net::LOAD_NORMAL| is appropriate.
  void Start(net::URLRequestContextGetter* request_context,
             const std::string& referrer,
             net::URLRequest::ReferrerPolicy referrer_policy,
             int load_flags);

  // Methods inherited from URLFetcherDelegate

  // This will be called when the URL has been fetched, successfully or not.
  // Use accessor methods on |source| to get the results.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // This will be called when some part of the response is read. |current|
  // denotes the number of bytes received up to the call, and |total| is the
  // expected total size of the response (or -1 if not determined).
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current,
                                          int64 total) OVERRIDE;

  // Methods inherited from ImageDecoder::Delegate

  // Called when image is decoded. |decoder| is used to identify the image in
  // case of decoding several images simultaneously.  This will not be called
  // on the UI thread.
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;

  // Called when decoding image failed.
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

 private:
  // Alerts the delegate that a failure occurred.
  void ReportFailure();

  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_refptr<ImageDecoder> image_decoder_;
  const GURL url_;
  BitmapFetcherDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcher);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_H_
