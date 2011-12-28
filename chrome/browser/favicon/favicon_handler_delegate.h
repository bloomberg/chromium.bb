// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_FAVICON_FAVICON_HANDLER_DELEGATE_H_
#pragma once

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

  // Starts the download for the given favicon.  When finished, the delegate
  // will call |OnDidDownloadFavicon()| with the results.
  virtual void StartDownload(int id, const GURL& url, int image_size) = 0;

  // Notifies the delegate that the favicon for the active entry was updated.
  virtual void NotifyFaviconUpdated() = 0;
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_HANDLER_DELEGATE_H_
