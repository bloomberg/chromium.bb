// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_

#include <vector>

#include "app/sql/connection.h"
#include "app/sql/init_status.h"
#include "app/sql/meta_table.h"
#include "base/ref_counted.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_database.h"  // For DBCloseScoper.

class FilePath;
class RefCountedMemory;
class SkBitmap;

namespace base {
class Time;
}

namespace history {

// Interface to be implemented by the real storage layer as well as
// the mockup database for testing.
class TopSitesDatabase {
 public:
  virtual ~TopSitesDatabase() {}

  // Returns a list of all URLs currently in the table.
  virtual MostVisitedURLList GetTopURLs() = 0;

  // Set a thumbnail for a URL. |url_rank| is the position of the URL
  // in the list of TopURLs, zero-based.
  // If the URL is not in the table, add it. If it is, replace its
  // thumbnail.
  virtual void SetPageThumbnail(const MostVisitedURL& url,
                                int url_rank,
                                const TopSites::Images thumbnail) = 0;

  // Get a thumbnail for a given page. Returns true iff we have the thumbnail.
  virtual bool GetPageThumbnail(const MostVisitedURL& url,
                                TopSites::Images* thumbnail) const = 0;

  // Remove the record for this URL. Returns true iff removed successfully.
  virtual bool RemoveURL(const MostVisitedURL& url) = 0;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_

