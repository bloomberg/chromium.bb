// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_DELEGATE_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_DELEGATE_H_

class GURL;

namespace content {
// TODO(jif): Abstract the NavigationEntry (crbug.com/359598).
class NavigationEntry;
}

// This class provides a delegate interface for a FaviconHandler.  It allows the
// FaviconHandler to ask its delegate for information or notify its delegate
// about changes.
class FaviconHandlerDelegate {
 public:
  // Returns the current NavigationEntry.
  // TODO(jif): Abstract the NavigationEntry (crbug.com/359598).
  virtual content::NavigationEntry* GetActiveEntry() = 0;

  // Starts the download for the given favicon. When finished, the delegate
  // will call OnDidDownloadFavicon() with the results.
  // Returns the unique id of the download request. The id will be passed
  // in OnDidDownloadFavicon().
  // Bitmaps with pixel sizes larger than |max_bitmap_size| are filtered out
  // from the bitmap results. If there are no bitmap results <=
  // |max_bitmap_size|, the smallest bitmap is resized to |max_bitmap_size| and
  // is the only result. A |max_bitmap_size| of 0 means unlimited.
  virtual int StartDownload(const GURL& url, int max_bitmap_size) = 0;

  // Notifies the delegate that the favicon for the active entry was updated.
  // |icon_url_changed| is true if a favicon with a different icon URL has
  // been selected since the previous call to NotifyFaviconUpdated().
  virtual void NotifyFaviconUpdated(bool icon_url_changed) = 0;

  // Returns whether the user is operating in an off-the-record context.
  virtual bool IsOffTheRecord() = 0;
};

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_DELEGATE_H_
