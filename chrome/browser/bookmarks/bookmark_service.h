// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_SERVICE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_SERVICE_H_
#pragma once

#include <vector>

class GURL;

// BookmarkService provides a thread safe view of bookmarks. It is used by
// HistoryBackend when it needs to determine the set of bookmarked URLs
// or if a URL is bookmarked.
//
// BookmarkService is owned by Profile and deleted when the Profile is deleted.
class BookmarkService {
 public:
  // Returns true if the specified URL is bookmarked.
  //
  // If not on the main thread you *must* invoke BlockTillLoaded first.
  virtual bool IsBookmarked(const GURL& url) = 0;

  // Returns, by reference in urls, the set of bookmarked urls. This returns
  // the unique set of URLs. For example, if two bookmarks reference the same
  // URL only one entry is added.
  //
  // If not on the main thread you *must* invoke BlockTillLoaded first.
  virtual void GetBookmarks(std::vector<GURL>* urls) = 0;

  // Blocks until loaded. This is intended for usage on a thread other than
  // the main thread.
  virtual void BlockTillLoaded() = 0;

 protected:
  virtual ~BookmarkService() {}
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_SERVICE_H_
