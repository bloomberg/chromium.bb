// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_

#include <map>
#include <string>
#include <vector>

#include "app/sql/connection.h"
#include "base/ref_counted.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_database.h"  // For DBCloseScoper.

class FilePath;
class RefCountedMemory;
class SkBitmap;
class TopSites;

namespace base {
class Time;
}

namespace history {

// Interface to be implemented by the real storage layer as well as
// the mockup database for testing.
class TopSitesDatabase {
 public:
  virtual ~TopSitesDatabase() {}
  virtual bool Init(const FilePath& filename) {
    return true;
  }

  // Returns a list of all URLs currently in the table.
  virtual void GetPageThumbnails(MostVisitedURLList* urls,
                                 std::map<GURL,
                                 TopSites::Images>* thumbnails) = 0;

  // Set a thumbnail for a URL. |url_rank| is the position of the URL
  // in the list of TopURLs, zero-based.
  // If the URL is not in the table, add it. If it is, replace its
  // thumbnail.
  virtual void SetPageThumbnail(const MostVisitedURL& url,
                                int url_rank,
                                const TopSites::Images& thumbnail) = 0;

  // Update rank of a URL that's already in the database.
  virtual void UpdatePageRank(const MostVisitedURL& url, int new_rank) = 0;

  // Convenience wrapper.
  bool GetPageThumbnail(const MostVisitedURL& url,
                        TopSites::Images* thumbnail) {
    return GetPageThumbnail(url.url, thumbnail);
  }

  // Get a thumbnail for a given page. Returns true iff we have the thumbnail.
  virtual bool GetPageThumbnail(const GURL& url,
                                TopSites::Images* thumbnail) = 0;

  // Remove the record for this URL. Returns true iff removed successfully.
  virtual bool RemoveURL(const MostVisitedURL& url) = 0;
};

class TopSitesDatabaseImpl : public TopSitesDatabase {
 public:
  TopSitesDatabaseImpl();
  ~TopSitesDatabaseImpl() {}

  // Must be called after creation but before any other methods are called.
  // Returns true on success. If false, no other functions should be called.
  virtual bool Init(const FilePath& db_name);

  // Thumbnails ----------------------------------------------------------------

  // Returns a list of all URLs currently in the table.
  // WARNING: clears both input arguments.
  virtual void GetPageThumbnails(MostVisitedURLList* urls,
                                 std::map<GURL, TopSites::Images>* thumbnails);

  // Set a thumbnail for a URL. |url_rank| is the position of the URL
  // in the list of TopURLs, zero-based.
  // If the URL is not in the table, add it. If it is, replace its
  // thumbnail and rank. Shift the ranks of other URLs if necessary.
  virtual void SetPageThumbnail(const MostVisitedURL& url,
                                int new_rank,
                                const TopSites::Images& thumbnail);

  // Sets the rank for a given URL. The URL must be in the database.
  // Use SetPageThumbnail if it's not.
  virtual void UpdatePageRank(const MostVisitedURL& url, int new_rank);

  // Get a thumbnail for a given page. Returns true iff we have the thumbnail.
  virtual bool GetPageThumbnail(const GURL& url,
                                TopSites::Images* thumbnail);

  // Remove the record for this URL. Returns true iff removed successfully.
  virtual bool RemoveURL(const MostVisitedURL& url);

 private:
  // Creates the thumbnail table, returning true if the table already exists
  // or was successfully created.
  bool InitThumbnailTable();

  // Adds a new URL to the database.
  void AddPageThumbnail(const MostVisitedURL& url,
                        int new_rank,
                        const TopSites::Images& thumbnail);

  // Sets the page rank. Should be called within an open transaction.
  void UpdatePageRankNoTransaction(const MostVisitedURL& url, int new_rank);

  // Updates thumbnail of a URL that's already in the database.
  void UpdatePageThumbnail(const MostVisitedURL& url,
                           const TopSites::Images& thumbnail);

  // Returns the URL's current rank or -1 if it is not present.
  int GetURLRank(const MostVisitedURL& url);

  // Returns the number of URLs (rows) in the database.
  int GetRowCount();

  // Encodes redirects into a string.
  static std::string GetRedirects(const MostVisitedURL& url);

  // Decodes redirects from a string and sets them for the url.
  static void SetRedirects(const std::string& redirects, MostVisitedURL* url);

  sql::Connection db_;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_
