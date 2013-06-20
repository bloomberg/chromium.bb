// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_FAVICON_FAVICON_HANDLER_DELEGATE_H_

class GURL;

namespace content {
class NavigationEntry;
}

// This class provides a delegate interface for a FaviconHandler.  It allows the
// FaviconHandler to ask its delegate for information or notify its delegate
// about changes.
class FaviconHandlerDelegate {
 public:
  // Returns the current NavigationEntry.
  virtual content::NavigationEntry* GetActiveEntry() = 0;

  // Starts the download for the given favicon. When finished, the delegate
  // will call OnDidDownloadFavicon() with the results.
  // Returns the unique id of the download request. The id will be passed
  // in OnDidDownloadFavicon().
  // |preferred_image_size| is used to select the size of the returned image if
  // the |url| is a multi resolution image.
  // |max_image_size| is the maximal size of the image. If the image at |url| is
  // bigger than this, it will be resized. 0 means unlimited.
  virtual int StartDownload(const GURL& url,
                            int preferred_image_size,
                            int max_image_size) = 0;

  // Notifies the delegate that the favicon for the active entry was updated.
  // |icon_url_changed| is true if a favicon with a different icon URL has
  // been selected since the previous call to NotifyFaviconUpdated().
  virtual void NotifyFaviconUpdated(bool icon_url_changed) = 0;
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_HANDLER_DELEGATE_H_
