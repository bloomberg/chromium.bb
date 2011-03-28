// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_
#pragma once

#include <map>
#include <string>

#include "app/sql/meta_table.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_database.h"  // For DBCloseScoper.

class FilePath;
class RefCountedMemory;
class SkBitmap;

namespace app {
class Connection;
}

namespace history {

class TopSitesDatabase {
 public:
  TopSitesDatabase();
  ~TopSitesDatabase();

  // Must be called after creation but before any other methods are called.
  // Returns true on success. If false, no other functions should be called.
  bool Init(const FilePath& db_name);

  // Returns true if migration of top sites from history may be needed. A value
  // of true means either migration is definitely needed (the top sites file is
  // old) or doesn't exist (as would happen for a new user).
  bool may_need_history_migration() const {
    return may_need_history_migration_;
  }

  // Thumbnails ----------------------------------------------------------------

  // Returns a list of all URLs currently in the table.
  // WARNING: clears both input arguments.
  void GetPageThumbnails(MostVisitedURLList* urls,
                         std::map<GURL, Images>* thumbnails);

  // Set a thumbnail for a URL. |url_rank| is the position of the URL
  // in the list of TopURLs, zero-based.
  // If the URL is not in the table, add it. If it is, replace its
  // thumbnail and rank. Shift the ranks of other URLs if necessary.
  void SetPageThumbnail(const MostVisitedURL& url,
                        int new_rank,
                        const Images& thumbnail);

  // Sets the rank for a given URL. The URL must be in the database.
  // Use SetPageThumbnail if it's not.
  void UpdatePageRank(const MostVisitedURL& url, int new_rank);

  // Get a thumbnail for a given page. Returns true iff we have the thumbnail.
  bool GetPageThumbnail(const GURL& url, Images* thumbnail);

  // Remove the record for this URL. Returns true iff removed successfully.
  bool RemoveURL(const MostVisitedURL& url);

 private:
  // Creates the thumbnail table, returning true if the table already exists
  // or was successfully created.
  bool InitThumbnailTable();

  // Adds a new URL to the database.
  void AddPageThumbnail(const MostVisitedURL& url,
                        int new_rank,
                        const Images& thumbnail);

  // Sets the page rank. Should be called within an open transaction.
  void UpdatePageRankNoTransaction(const MostVisitedURL& url, int new_rank);

  // Updates thumbnail of a URL that's already in the database.
  void UpdatePageThumbnail(const MostVisitedURL& url,
                           const Images& thumbnail);

  // Returns the URL's current rank or -1 if it is not present.
  int GetURLRank(const MostVisitedURL& url);

  // Returns the number of URLs (rows) in the database.
  int GetRowCount();

  sql::Connection* CreateDB(const FilePath& db_name);

  // Encodes redirects into a string.
  static std::string GetRedirects(const MostVisitedURL& url);

  // Decodes redirects from a string and sets them for the url.
  static void SetRedirects(const std::string& redirects, MostVisitedURL* url);

  scoped_ptr<sql::Connection> db_;
  sql::MetaTable meta_table_;

  // See description above class.
  bool may_need_history_migration_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_DATABASE_H_
