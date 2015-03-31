// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_

class GURL;

namespace gfx {
class Image;
}

// Interface that allows Favicon core code to interact with its driver (i.e.,
// obtain information from it and give information to it). A concrete
// implementation must be provided by the driver.
class FaviconDriver {
 public:
  // Starts the download for the given favicon. When finished, the driver
  // will call OnDidDownloadFavicon() with the results.
  // Returns the unique id of the download request. The id will be passed
  // in OnDidDownloadFavicon().
  // Bitmaps with pixel sizes larger than |max_bitmap_size| are filtered out
  // from the bitmap results. If there are no bitmap results <=
  // |max_bitmap_size|, the smallest bitmap is resized to |max_bitmap_size| and
  // is the only result. A |max_bitmap_size| of 0 means unlimited.
  virtual int StartDownload(const GURL& url, int max_bitmap_size) = 0;

  // Returns whether the user is operating in an off-the-record context.
  virtual bool IsOffTheRecord() = 0;

  // Returns the bitmap of the current page's favicon. Requires GetActiveURL()
  // to be valid.
  virtual const gfx::Image GetActiveFaviconImage() = 0;

  // Returns the URL of the current page's favicon. Requires GetActiveURL() to
  // be valid.
  virtual const GURL GetActiveFaviconURL() = 0;

  // Returns whether the page's favicon is valid (returns false if the default
  // favicon is being used). Requires GetActiveURL() to be valid.
  virtual bool GetActiveFaviconValidity() = 0;

  // Returns the URL of the current page, if any. Returns an invalid
  // URL otherwise.
  virtual const GURL GetActiveURL() = 0;

  // Notifies the driver a favicon image is available. |image| is not
  // necessarily 16x16. |icon_url| is the url the image is from. If
  // |is_active_favicon| is true the image corresponds to the favicon
  // (possibly empty) of the page.
  virtual void OnFaviconAvailable(const gfx::Image& image,
                                  const GURL& icon_url,
                                  bool is_active_favicon) = 0;

 protected:
  FaviconDriver() {}
  virtual ~FaviconDriver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FaviconDriver);
};

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_
