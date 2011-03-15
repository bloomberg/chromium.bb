// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H_
#define CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H_
#pragma once

#include <vector>

#include "app/sql/connection.h"
#include "app/sql/init_status.h"
#include "app/sql/meta_table.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/history/history_types.h"

class FilePath;
class RefCountedMemory;
struct ThumbnailScore;
class SkBitmap;

namespace base {
class Time;
}

namespace history {

class ExpireHistoryBackend;
class HistoryPublisher;

// This database interface is owned by the history backend and runs on the
// history thread. It is a totally separate component from history partially
// because we may want to move it to its own thread in the future. The
// operations we will do on this database will be slow, but we can tolerate
// higher latency (it's OK for thumbnails to come in slower than the rest
// of the data). Moving this to a separate thread would not block potentially
// higher priority history operations.
class ThumbnailDatabase {
 public:
  ThumbnailDatabase();
  ~ThumbnailDatabase();

  // Must be called after creation but before any other methods are called.
  // When not INIT_OK, no other functions should be called.
  sql::InitStatus Init(const FilePath& db_name,
                       const HistoryPublisher* history_publisher,
                       URLDatabase* url_database);

  // Open database on a given filename. If the file does not exist,
  // it is created.
  // |db| is the database to open.
  // |db_name| is a path to the database file.
  static sql::InitStatus OpenDatabase(sql::Connection* db,
                                      const FilePath& db_name);

  // Transactions on the database.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const {
    return db_.transaction_nesting();
  }

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Thumbnails ----------------------------------------------------------------

  // Sets the given data to be the thumbnail for the given URL,
  // overwriting any previous data. If the SkBitmap contains no pixel
  // data, the thumbnail will be deleted.
  void SetPageThumbnail(const GURL& url,
                        URLID id,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score,
                        base::Time time);

  // Retrieves thumbnail data for the given URL, returning true on success,
  // false if there is no such thumbnail or there was some other error.
  bool GetPageThumbnail(URLID id, std::vector<unsigned char>* data);

  // Delete the thumbnail with the provided id. Returns false on failure
  bool DeleteThumbnail(URLID id);

  // If there is a thumbnail score for the id provided, retrieves the
  // current thumbnail score and places it in |score| and returns
  // true. Returns false otherwise.
  bool ThumbnailScoreForId(URLID id, ThumbnailScore* score);

  // Called by the to delete all old thumbnails and make a clean table.
  // Returns true on success.
  bool RecreateThumbnailTable();

  // Favicons ------------------------------------------------------------------

  // Sets the bits for a favicon. This should be png encoded data.
  // The time indicates the access time, and is used to detect when the favicon
  // should be refreshed.
  bool SetFavicon(FaviconID icon_id,
                  scoped_refptr<RefCountedMemory> icon_data,
                  base::Time time);

  // Sets the time the favicon was last updated.
  bool SetFaviconLastUpdateTime(FaviconID icon_id, base::Time time);

  // Returns the id of the entry in the favicon database with the specified url
  // and icon type. If |required_icon_type| contains multiple icon types and
  // there are more than one matched icon in database, only one icon will be
  // returned in the priority of TOUCH_PRECOMPOSED_ICON, TOUCH_ICON, and
  // FAVICON, and the icon type is returned in icon_type parameter if it is not
  // NULL.
  // Returns 0 if no entry exists for the specified url.
  FaviconID GetFaviconIDForFaviconURL(const GURL& icon_url,
                                      int required_icon_type,
                                      IconType* icon_type);

  // Gets the png encoded favicon and last updated time for the specified
  // favicon id.
  bool GetFavicon(FaviconID icon_id,
                  base::Time* last_updated,
                  std::vector<unsigned char>* png_icon_data,
                  GURL* icon_url);

  // Adds the favicon URL and icon type to the favicon db, returning its id.
  FaviconID AddFavicon(const GURL& icon_url, IconType icon_type);

  // Delete the favicon with the provided id. Returns false on failure
  bool DeleteFavicon(FaviconID id);

  // Icon Mapping --------------------------------------------------------------
  //
  // Returns true if there is a matched icon mapping for the given page and
  // icon type.
  // The matched icon mapping is returned in the icon_mapping parameter if it is
  // not NULL.
  bool GetIconMappingForPageURL(const GURL& page_url,
                                IconType required_icon_type,
                                IconMapping* icon_mapping);

  // Returns true if there is any matched icon mapping for the given page.
  // All matched icon mappings are returned in descent order of IconType if
  // mapping_data is not NULL.
  bool GetIconMappingsForPageURL(const GURL& page_url,
                                 std::vector<IconMapping>* mapping_data);

  // Adds a mapping between the given page_url and icon_id.
  // Returns the new mapping id if the adding succeeds, otherwise 0 is returned.
  IconMappingID AddIconMapping(const GURL& page_url, FaviconID icon_id);

  // Updates the page and icon mapping for the given mapping_id with the given
  // icon_id.
  // Returns true if the update succeeded.
  bool UpdateIconMapping(IconMappingID mapping_id, FaviconID icon_id);

  // Deletes the icon mapping entries for the given page url.
  // Returns true if the deletion succeeded.
  bool DeleteIconMappings(const GURL& page_url);

  // Checks whether a favicon is used by any URLs in the database.
  bool HasMappingFor(FaviconID id);

  // Temporary IconMapping -----------------------------------------------------
  //
  // Creates a temporary table to store icon mapping. Icon mapping will be
  // copied to this table by AddToTemporaryIconMappingTable() and then the
  // original table will be dropped, leaving only those copied mapping
  // remaining. This is used to quickly delete most of the icon mapping when
  // clearing history.
  bool InitTemporaryIconMappingTable() {
    return InitIconMappingTable(&db_, true);
  }

  // Copies the given icon mapping from the "main" icon_mapping table to the
  // temporary one. This is only valid in between calls to
  // InitTemporaryIconMappingTable()
  // and CommitTemporaryIconMappingTable().
  //
  // The ID of the favicon will change when this copy takes place. The new ID
  // is returned, or 0 on failure.
  IconMappingID AddToTemporaryIconMappingTable(const GURL& page_url,
                                               const FaviconID icon_id);

  // Replaces the main icon mapping table with the temporary table created by
  // InitTemporaryIconMappingTable(). This will mean all icon mapping not copied
  // over will be deleted. Returns true on success.
  bool CommitTemporaryIconMappingTable();

  // Temporary Favicons --------------------------------------------------------

  // Create a temporary table to store favicons. Favicons will be copied to
  // this table by CopyToTemporaryFaviconTable() and then the original table
  // will be dropped, leaving only those copied favicons remaining. This is
  // used to quickly delete most of the favicons when clearing history.
  bool InitTemporaryFaviconsTable() {
    return InitFaviconsTable(&db_, true);
  }

  // Copies the given favicon from the "main" favicon table to the temporary
  // one. This is only valid in between calls to InitTemporaryFaviconsTable()
  // and CommitTemporaryFaviconTable().
  //
  // The ID of the favicon will change when this copy takes place. The new ID
  // is returned, or 0 on failure.
  FaviconID CopyToTemporaryFaviconTable(FaviconID source);

  // Replaces the main URL table with the temporary table created by
  // InitTemporaryFaviconsTable(). This will mean all favicons not copied over
  // will be deleted. Returns true on success.
  bool CommitTemporaryFaviconTable();

  // Returns true iff the thumbnails table exists.
  // Migrating to TopSites is dropping the thumbnails table.
  bool NeedsMigrationToTopSites();

  // Renames the database file and drops the Thumbnails table.
  bool RenameAndDropThumbnails(const FilePath& old_db_file,
                               const FilePath& new_db_file);

 private:
  friend class ExpireHistoryBackend;
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, UpgradeToVersion4);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MigrationIconMapping);

  // Creates the thumbnail table, returning true if the table already exists
  // or was successfully created.
  bool InitThumbnailTable();

  // Creates the favicon table, returning true if the table already exists,
  // or was successfully created. |is_temporary| will be false when generating
  // the "regular" favicons table. The expirer sets this to true to generate the
  // temporary table, which will have a different name but the same schema.
  // |db| is the connection to use for initializing the table.
  // A different connection is used in RenameAndDropThumbnails, when we
  // need to copy the favicons between two database files.
  bool InitFaviconsTable(sql::Connection* db, bool is_temporary);

  // Adds support for the new metadata on web page thumbnails.
  bool UpgradeToVersion3();

  // Adds support for the icon_type in favicon table.
  bool UpgradeToVersion4();

  // Migrates the icon mapping data from URL database to Thumbnail database.
  // Return whether the migration succeeds.
  bool MigrateIconMappingData(URLDatabase* url_db);

  // Creates the index over the favicon table. This will be called during
  // initialization after the table is created. This is a separate function
  // because it is used by SwapFaviconTables to create an index over the
  // newly-renamed favicons table (formerly the temporary table with no index).
  void InitFaviconsIndex();

  // Creates the icon_map table, return true if the table already exists or was
  // successfully created.
  bool InitIconMappingTable(sql::Connection* db, bool is_temporary);

  // Creates the index over the icon_mapping table, This will be called during
  // initialization after the table is created. This is a separate function
  // because it is used by CommitTemporaryIconMappingTable to create an index
  // over the newly-renamed icon_mapping table (formerly the temporary table
  // with no index).
  void InitIconMappingIndex();

  // Adds a mapping between the given page_url and icon_id; The mapping will be
  // added to temp_icon_mapping table if is_temporary is true.
  // Returns the new mapping id if the adding succeeds, otherwise 0 is returned.
  IconMappingID AddIconMapping(const GURL& page_url,
                               FaviconID icon_id,
                               bool is_temporary);

  sql::Connection db_;
  sql::MetaTable meta_table_;

  // This object is created and managed by the history backend. We maintain an
  // opaque pointer to the object for our use.
  // This can be NULL if there are no indexers registered to receive indexing
  // data from us.
  const HistoryPublisher* history_publisher_;

  // True if migration to TopSites has been done and the thumbnails
  // table should not be used.
  bool use_top_sites_;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H_
