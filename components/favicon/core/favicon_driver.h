// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"

class GURL;

namespace gfx {
class Image;
}

namespace favicon {

class FaviconDriverObserver;

// Interface that allows favicon core code to obtain information about the
// current page. This is partially implemented by FaviconDriverImpl, and
// concrete implementation should be based on that class instead of directly
// subclassing FaviconDriver.
class FaviconDriver {
 public:
  // Adds/Removes an observer.
  void AddObserver(FaviconDriverObserver* observer);
  void RemoveObserver(FaviconDriverObserver* observer);

  // Initiates loading the favicon for the specified url.
  virtual void FetchFavicon(const GURL& url) = 0;

  // Returns the favicon for this tab, or IDR_DEFAULT_FAVICON if the tab does
  // not have a favicon. The default implementation uses the current navigation
  // entry. Returns an empty bitmap if there are no navigation entries, which
  // should rarely happen.
  virtual gfx::Image GetFavicon() const = 0;

  // Returns true if we have the favicon for the page.
  virtual bool FaviconIsValid() const = 0;

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

  // Returns whether |url| is bookmarked.
  virtual bool IsBookmarked(const GURL& url) = 0;

  // Returns the URL of the current page, if any. Returns an invalid URL
  // otherwise.
  virtual GURL GetActiveURL() = 0;

  // Sets whether the page's favicon is valid (if false, the default favicon is
  // being used).
  virtual void SetActiveFaviconValidity(bool valid) = 0;

  // Returns the URL of the current page's favicon.
  virtual GURL GetActiveFaviconURL() = 0;

  // Sets the URL of the favicon's bitmap.
  virtual void SetActiveFaviconURL(const GURL& url) = 0;

  // Sets the bitmap of the current page's favicon.
  virtual void SetActiveFaviconImage(const gfx::Image& image) = 0;

  // Notifies the driver a favicon image is available. |image| is not
  // necessarily 16x16. |icon_url| is the url the image is from. If
  // |is_active_favicon| is true the image corresponds to the favicon
  // (possibly empty) of the page.
  virtual void OnFaviconAvailable(const GURL& page_url,
                                  const GURL& icon_url,
                                  const gfx::Image& image,
                                  bool is_active_favicon) = 0;

  // Returns whether the driver is waiting for a download to complete or for
  // data from the FaviconService. Reserved for testing.
  virtual bool HasPendingTasksForTest() = 0;

 protected:
  FaviconDriver();
  virtual ~FaviconDriver();

  // Informs FaviconDriverObservers that favicon |image| has been retrieved from
  // either website or cached storage.
  void NotifyFaviconAvailable(const gfx::Image& image);

  // Informs FaviconDriverObservers that favicon has changed for the current
  // page. |icon_url_changed| is true if the favicon URL has also changed since
  // the last call.
  virtual void NotifyFaviconUpdated(bool icon_url_changed);

 private:
  base::ObserverList<FaviconDriverObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FaviconDriver);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_
