// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_IMAGE_FETCHER_H_
#define IOS_CHROME_BROWSER_NET_IMAGE_FETCHER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

@class NSData;

namespace image_fetcher {

// Callback that informs of the download of an image encoded in |data|,
// downloaded from |url|, and with the http status |http_response_code|. If the
// url is a data URL, |http_response_code| is always 200.
typedef void (^Callback)(const GURL& url, int http_response_code, NSData* data);

// Utility class that will retrieve an image from an URL. The image is returned
// as NSData which can be used with +[UIImage imageWithData:]. This class
// usually returns the raw bytes retrieved from the network without any
// processing with the exception of WebP encoded images. Those are decoded and
// then reencoded in a format suitable for UIImage.
// An instance of this class can download a number of images at the same time.
class ImageFetcher : public net::URLFetcherDelegate {
 public:
  // The WorkerPool is used to eventually decode the image.
  explicit ImageFetcher(
      const scoped_refptr<base::SequencedWorkerPool> decoding_pool);
  ~ImageFetcher() override;

  // Start downloading the image at the given |url|. The |callback| will be
  // called with the downloaded image, or nil if any error happened. The
  // |referrer| and |referrer_policy| will be passed on to the underlying
  // URLFetcher.
  // This method assumes the request context getter has been set.
  // (virtual for testing)
  virtual void StartDownload(const GURL& url,
                             Callback callback,
                             const std::string& referrer,
                             net::URLRequest::ReferrerPolicy referrer_policy);

  // Helper method to call StartDownload without a referrer.
  // (virtual for testing)
  virtual void StartDownload(const GURL& url, Callback callback);

  // A valid request context getter is required before starting the download.
  // (virtual for testing)
  virtual void SetRequestContextGetter(
      net::URLRequestContextGetter* request_context_getter);

  // content::URLFetcherDelegate methods.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  // Runs the callback with the given arguments.
  void RunCallback(const base::mac::ScopedBlock<Callback>& callback,
                   const GURL& url,
                   const int http_response_code,
                   NSData* data);

  // Tracks open download requests.  The key is the URLFetcher object doing the
  // fetch; the value is the callback to use when the download request
  // completes. When a download request completes, the URLFetcher must be
  // deleted and the callback called and released.
  std::map<const net::URLFetcher*, Callback> downloads_in_progress_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // The WeakPtrFactory is used to cancel callbacks if ImageFetcher is destroyed
  // during WebP decoding.
  base::WeakPtrFactory<ImageFetcher> weak_factory_;

  // The pool used to decode images if necessary.
  const scoped_refptr<base::SequencedWorkerPool> decoding_pool_;
};

}  // namespace image_fetcher

#endif  // IOS_CHROME_BROWSER_NET_IMAGE_FETCHER_H_
