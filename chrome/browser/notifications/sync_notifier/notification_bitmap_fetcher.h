// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_NOTIFICATION_BITMAP_FETCHER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_NOTIFICATION_BITMAP_FETCHER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/image_decoder.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
}  // namespace net

class Profile;

namespace notifier {

// A delegate interface for users of NotificationBitmapFetcher.
class NotificationBitmapFetcherDelegate {
 public:
  // This will be called when the bitmap has been requested, whether or not the
  // request succeeds.  |url| is the URL that was originally fetched so we can
  // match up the bitmap with a specific request.
  virtual void OnFetchComplete(const GURL url, const SkBitmap* bitmap) = 0;

 protected:
  virtual ~NotificationBitmapFetcherDelegate() {}
};

class NotificationBitmapFetcher
    : public net::URLFetcherDelegate,
      public ImageDecoder::Delegate {
 public:
  NotificationBitmapFetcher(
      const GURL& url,
      NotificationBitmapFetcherDelegate* delegate);
  virtual ~NotificationBitmapFetcher();

  GURL url() const { return url_; }

  // Start fetching the URL with the fetcher.  The operation will be continued
  // in the OnURLFetchComplete callback.
  void Start(Profile* profile);

  // Methods inherited from URLFetcherDelegate

  // This will be called when the URL has been fetched, successfully or not.
  // Use accessor methods on |source| to get the results.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // This will be called when some part of the response is read. |current|
  // denotes the number of bytes received up to the call, and |total| is the
  // expected total size of the response (or -1 if not determined).
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current, int64 total) OVERRIDE;

  // Methods inherited from ImageDecoder::Delegate

  // Called when image is decoded. |decoder| is used to identify the image in
  // case of decoding several images simultaneously.  This will not be called
  // on the UI thread.
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;

  // Called when decoding image failed.
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

 private:
  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_refptr<ImageDecoder> image_decoder_;
  const GURL url_;
  scoped_ptr<SkBitmap> bitmap_;
  NotificationBitmapFetcherDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBitmapFetcher);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_NOTIFICATION_BITMAP_FETCHER_H_
