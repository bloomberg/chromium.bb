// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H_
#define CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/history/history_types.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

class FilePath;
struct ThumbnailScore;
class SkBitmap;

namespace base {
class RefCountedMemory;
class Time;
}

namespace gfx {
class Image;
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
  void RollbackTransaction();

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Thumbnails ----------------------------------------------------------------

  // Sets the given data to be the thumbnail for the given URL,
  // overwriting any previous data. If the SkBitmap contains no pixel
  // data, the thumbnail will be deleted.
  bool SetPageThumbnail(const GURL& url,
                        URLID id,
                        const gfx::Image* thumbnail,
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

  // Favicon Bitmaps -----------------------------------------------------------

  // Returns true if there are favicon bitmaps for |icon_id|. If
  // |bitmap_id_sizes| is non NULL, sets it to a list of the favicon bitmap ids
  // and their associated pixel sizes for the favicon with |icon_id|.
  // The list contains results for the bitmaps which are cached in the
  // favicon_bitmaps table. The pixel sizes are a subset of the sizes in the
  // 'sizes' field of the favicons table for |icon_id|.
  bool GetFaviconBitmapIDSizes(
      FaviconID icon_id,
      std::vector<FaviconBitmapIDSize>* bitmap_id_sizes);

  // Returns true if there are any matched bitmaps for the given |icon_id|. All
  // matched results are returned if |favicon_bitmaps| is not NULL.
  bool GetFaviconBitmaps(FaviconID icon_id,
                         std::vector<FaviconBitmap>* favicon_bitmaps);

  // Gets the last updated time, bitmap data, and pixel size of the favicon
  // bitmap at |bitmap_id|. Returns true if successful.
  bool GetFaviconBitmap(FaviconBitmapID bitmap_id,
                        base::Time* last_updated,
                        scoped_refptr<base::RefCountedMemory>* png_icon_data,
                        gfx::Size* pixel_size);

  // Adds a bitmap component at |pixel_size| for the favicon with |icon_id|.
  // Only favicons representing a .ico file should have multiple favicon bitmaps
  // per favicon.
  // |icon_data| is the png encoded data.
  // The |time| indicates the access time, and is used to detect when the
  // favicon should be refreshed.
  // |pixel_size| is the pixel dimensions of |icon_data|.
  // Returns the id of the added bitmap or 0 if unsuccessful.
  FaviconBitmapID AddFaviconBitmap(
      FaviconID icon_id,
      const scoped_refptr<base::RefCountedMemory>& icon_data,
      base::Time time,
      const gfx::Size& pixel_size);

  // Sets the bitmap data and the last updated time for the favicon bitmap at
  // |bitmap_id|.
  // Returns true if successful.
  bool SetFaviconBitmap(FaviconBitmapID bitmap_id,
                        scoped_refptr<base::RefCountedMemory> bitmap_data,
                        base::Time time);

  // Deletes the favicon bitmaps for the favicon with with |icon_id|.
  // Returns true if successful.
  bool DeleteFaviconBitmapsForFavicon(FaviconID icon_id);

  // Deletes the favicon bitmap with |bitmap_id|.
  // Returns true if successful.
  bool DeleteFaviconBitmap(FaviconBitmapID bitmap_id);

  // Favicons ------------------------------------------------------------------

  // Updates the favicon sizes associated with a favicon to |favicon_sizes|.
  // See comment in history_types.h for description of |favicon_sizes|.
  bool SetFaviconSizes(FaviconID icon_id, const FaviconSizes& favicon_sizes);

  // Sets the the favicon as out of date. This will set |last_updated| for all
  // of the bitmaps for |icon_id| to be out of date.
  bool SetFaviconOutOfDate(FaviconID icon_id);

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

  // Gets the icon_url, icon_type and sizes for the specified |icon_id|.
  bool GetFaviconHeader(FaviconID icon_id,
                        GURL* icon_url,
                        IconType* icon_type,
                        FaviconSizes* favicon_sizes);

  // Adds favicon with |icon_url|, |icon_type| and |favicon_sizes| to the
  // favicon db, returning its id.
  FaviconID AddFavicon(const GURL& icon_url,
                       IconType icon_type,
                       const FaviconSizes& favicon_sizes);

  // Adds a favicon with a single bitmap. This call is equivalent to calling
  // AddFavicon and AddFaviconBitmap.
  FaviconID AddFavicon(const GURL& icon_url,
                       IconType icon_type,
                       const FaviconSizes& favicon_sizes,
                       const scoped_refptr<base::RefCountedMemory>& icon_data,
                       base::Time time,
                       const gfx::Size& pixel_size);

  // Delete the favicon with the provided id. Returns false on failure
  bool DeleteFavicon(FaviconID id);

  // Icon Mapping --------------------------------------------------------------
  //
  // Returns true if there is a matched icon mapping for the given page and
  // icon type.
  // The matched icon mapping is returned in the icon_mapping parameter if it is
  // not NULL.

  // Returns true if there are icon mappings for the given page and icon types.
  // If |required_icon_types| contains multiple icon types and there is more
  // than one matched icon type in the database, icons of only a single type
  // will be returned in the priority of TOUCH_PRECOMPOSED_ICON, TOUCH_ICON,
  // and FAVICON.
  // The matched icon mappings are returned in the |mapping_data| parameter if
  // it is not NULL.
  bool GetIconMappingsForPageURL(const GURL& page_url,
                                 int required_icon_types,
                                 std::vector<IconMapping>* mapping_data);

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

  // Deletes the icon mapping with |mapping_id|.
  // Returns true if the deletion succeeded.
  bool DeleteIconMapping(IconMappingID mapping_id);

  // Checks whether a favicon is used by any URLs in the database.
  bool HasMappingFor(FaviconID id);

  // Clones the existing mappings from |old_page_url| if |new_page_url| has no
  // mappings. Otherwise, will leave mappings alone.
  bool CloneIconMappings(const GURL& old_page_url, const GURL& new_page_url);

  // The class to enumerate icon mappings. Use InitIconMappingEnumerator to
  // initialize.
  class IconMappingEnumerator {
   public:
    IconMappingEnumerator();
    ~IconMappingEnumerator();

    // Get the next icon mapping, return false if no more are available.
    bool GetNextIconMapping(IconMapping* icon_mapping);

   private:
    friend class ThumbnailDatabase;

    // Used to query database and return the data for filling IconMapping in
    // each call of GetNextIconMapping().
    sql::Statement statement_;

    DISALLOW_COPY_AND_ASSIGN(IconMappingEnumerator);
  };

  // Return all icon mappings of the given |icon_type|.
  bool InitIconMappingEnumerator(IconType type,
                                 IconMappingEnumerator* enumerator);

  // Temporary Tables ---------------------------------------------------------
  //
  // Creates empty temporary tables for each of the tables in the thumbnail
  // database. Favicon data which is not copied into the temporary tables will
  // be deleted when CommitTemporaryTables() is called. This is used to delete
  // most of the favicons when clearing history.
  bool InitTemporaryTables();

  // Replaces the main tables in the thumbnail database with the temporary
  // tables created with InitTemporaryTables(). This means that any data not
  // copied over will be deleted.
  bool CommitTemporaryTables();

  // Copies the given icon mapping from the "main" icon_mapping table to the
  // temporary one. This is only valid in between calls to
  // InitTemporaryTables() and CommitTemporaryTables().
  //
  // The ID of the favicon will change when this copy takes place. The new ID
  // is returned, or 0 on failure.
  IconMappingID AddToTemporaryIconMappingTable(const GURL& page_url,
                                               const FaviconID icon_id);

  // Copies the given favicon and associated favicon bitmaps from the "main"
  // favicon and favicon_bitmaps tables to the temporary ones. This is only
  // valid in between calls to InitTemporaryTables() and
  // CommitTemporaryTables().
  //
  // The ID of the favicon will change when this copy takes place. The new ID
  // is returned, or 0 on failure.
  FaviconID CopyFaviconAndFaviconBitmapsToTemporaryTables(FaviconID source);

  // Returns true iff the thumbnails table exists.
  // Migrating to TopSites is dropping the thumbnails table.
  bool NeedsMigrationToTopSites();

  // Renames the database file and drops the Thumbnails table.
  bool RenameAndDropThumbnails(const FilePath& old_db_file,
                               const FilePath& new_db_file);

 private:
  friend class ExpireHistoryBackend;
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest,
                           GetFaviconAfterMigrationToTopSites);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, UpgradeToVersion4);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, UpgradeToVersion5);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, UpgradeToVersion6);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, FaviconSizesToAndFromString);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MigrationIconMapping);

  // Creates the thumbnail table, returning true if the table already exists
  // or was successfully created.
  bool InitThumbnailTable();

  // Helper function to handle cleanup on upgrade failures.
  sql::InitStatus CantUpgradeToVersion(int cur_version);

  // Adds support for the new metadata on web page thumbnails.
  bool UpgradeToVersion3();

  // Adds support for the icon_type in favicon table.
  bool UpgradeToVersion4();

  // Adds support for sizes in favicon table.
  bool UpgradeToVersion5();

  // Adds support for size in favicons table and removes sizes column.
  bool UpgradeToVersion6();

  // Migrates the icon mapping data from URL database to Thumbnail database.
  // Return whether the migration succeeds.
  bool MigrateIconMappingData(URLDatabase* url_db);

  // Creates the favicon table, returning true if the table already exists,
  // or was successfully created. |is_temporary| will be false when generating
  // the "regular" favicons table. The expirer sets this to true to generate the
  // temporary table, which will have a different name but the same schema.
  // |db| is the connection to use for initializing the table.
  // A different connection is used in RenameAndDropThumbnails, when we
  // need to copy the favicons between two database files.
  bool InitFaviconsTable(sql::Connection* db, bool is_temporary);

  // Creates the index over the favicon table. This will be called during
  // initialization after the table is created. This is a separate function
  // because it is used by SwapFaviconTables to create an index over the
  // newly-renamed favicons table (formerly the temporary table with no index).
  bool InitFaviconsIndex();

  // Creates the favicon_bitmaps table, return true if the table already exists
  // or was successfully created.
  bool InitFaviconBitmapsTable(sql::Connection* db, bool is_temporary);

  // Creates the index over the favicon_bitmaps table. This will be called
  // during initialization after the table is created. This is a separate
  // function because it is used by CommitTemporaryTables to create an
  // index over the newly-renamed favicon_bitmaps table (formerly the temporary
  // table with no index).
  bool InitFaviconBitmapsIndex();

  // Creates the icon_map table, return true if the table already exists or was
  // successfully created.
  bool InitIconMappingTable(sql::Connection* db, bool is_temporary);

  // Creates the index over the icon_mapping table, This will be called during
  // initialization after the table is created. This is a separate function
  // because it is used by CommitTemporaryIconMappingTable to create an index
  // over the newly-renamed icon_mapping table (formerly the temporary table
  // with no index).
  bool InitIconMappingIndex();

  // Returns true if the |favicons| database is missing a column.
  bool IsFaviconDBStructureIncorrect();

  // Adds a mapping between the given page_url and icon_id; The mapping will be
  // added to temp_icon_mapping table if is_temporary is true.
  // Returns the new mapping id if the adding succeeds, otherwise 0 is returned.
  IconMappingID AddIconMapping(const GURL& page_url,
                               FaviconID icon_id,
                               bool is_temporary);

  // Returns True if the current database is latest.
  bool IsLatestVersion();

  // Converts the vector representation of favicon sizes as passed into
  // SetFaviconSizes to a string to store in the |favicons| database table.
  // Format:
  //   Each widthxheight pair is separated by a space.
  //   Width and height are separated by a space.
  // For instance, if sizes contains pixel sizes (16x16, 32x32), the
  // string representation is "16 16 32 32".
  static void FaviconSizesToDatabaseString(const FaviconSizes& favicon_sizes,
                                           std::string* favicon_sizes_string);

  // Converts the string representation of favicon sizes as stored in the
  // |favicons| database table to a vector. Returns an empty vector if there
  // were parsing errors.
  static void DatabaseStringToFaviconSizes(
      const std::string& favicon_sizes_string,
      FaviconSizes* favicon_sizes);

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
