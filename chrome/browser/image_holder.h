// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_HOLDER_H_
#define CHROME_BROWSER_IMAGE_HOLDER_H_

#include "base/memory/scoped_vector.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

class Profile;

namespace chrome {

// This provides a callback so the ImageHolder can inform its parent when a
// bitmap arrives.
class ImageHolderDelegate {
 public:
  virtual void OnFetchComplete() = 0;
};

// This class encapsulates the action of fetching a bitmap, reporting when it is
// fetched, and holding onto the bitmap until no longer needed.
class ImageHolder : public chrome::BitmapFetcherDelegate {
 public:
  ImageHolder(const GURL& low_dpi_url,
              const GURL& high_dpi_url,
              Profile* profile,
              ImageHolderDelegate* delegate);
  virtual ~ImageHolder();

  // Begin fetching of the URLs we have.
  void StartFetch();

  // Check whether we have a response from the server for these resources,
  // even if the response is a failed fetch.
  bool IsFetchingDone() const;

  // Inherited from BitmapFetcherDelegate
  virtual void OnFetchComplete(const GURL url, const SkBitmap* bitmap) OVERRIDE;

  // Accessors:
  GURL low_dpi_url() const { return low_dpi_url_; }
  GURL high_dpi_url() const { return high_dpi_url_; }
  gfx::Image low_dpi_image() { return gfx::Image(image_); }

 private:
  // Helper function to create a bitmap fetcher (but not start the fetch).
  void CreateBitmapFetcher(const GURL& url);

  GURL low_dpi_url_;
  GURL high_dpi_url_;
  bool low_dpi_fetched_;
  bool high_dpi_fetched_;
  gfx::ImageSkia image_;
  ImageHolderDelegate* delegate_;
  ScopedVector<chrome::BitmapFetcher> fetchers_;
  Profile* profile_;

  FRIEND_TEST_ALL_PREFIXES(ImageHolderTest, CreateBitmapFetcherTest);
  FRIEND_TEST_ALL_PREFIXES(ImageHolderTest, OnFetchCompleteTest);
  FRIEND_TEST_ALL_PREFIXES(ImageHolderTest, IsFetchingDoneTest);

  DISALLOW_COPY_AND_ASSIGN(ImageHolder);
};

}  // namespace chrome.

#endif  // CHROME_BROWSER_IMAGE_HOLDER_H_
