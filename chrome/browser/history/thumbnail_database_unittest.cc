// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "sql/connection.h"
#include "sql/recovery.h"  // For FullRecoverySupported().
#include "sql/statement.h"
#include "sql/test/scoped_error_ignorer.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/gurl.h"

namespace history {

namespace {

// Blobs for the bitmap tests.  These aren't real bitmaps.  Golden
// database files store the same blobs (see VersionN tests).
const unsigned char kBlob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";
const unsigned char kBlob2[] =
    "goiwuegrqrcomizqyzkjalitbahxfjytrqvpqeroicxmnlkhlzunacxaneviawrtxcywhgef";

// Page and icon urls shared by tests.  Present in golden database
// files (see VersionN tests).
const GURL kPageUrl1 = GURL("http://google.com/");
const GURL kPageUrl2 = GURL("http://yahoo.com/");
const GURL kPageUrl3 = GURL("http://www.google.com/");
const GURL kPageUrl4 = GURL("http://www.google.com/blank.html");

const GURL kIconUrl1 = GURL("http://www.google.com/favicon.ico");
const GURL kIconUrl2 = GURL("http://www.yahoo.com/favicon.ico");
const GURL kIconUrl3 = GURL("http://www.google.com/touch.ico");

const gfx::Size kSmallSize = gfx::Size(16, 16);
const gfx::Size kLargeSize = gfx::Size(32, 32);

// Create the test database at |db_path| from the golden file at
// |ascii_path| in the "History/" subdir of the test data dir.
WARN_UNUSED_RESULT bool CreateDatabaseFromSQL(const base::FilePath &db_path,
                                              const char* ascii_path) {
  base::FilePath sql_path;
  if (!PathService::Get(chrome::DIR_TEST_DATA, &sql_path))
    return false;
  sql_path = sql_path.AppendASCII("History").AppendASCII(ascii_path);
  return sql::test::CreateDatabaseFromSQL(db_path, sql_path);
}

int GetPageSize(sql::Connection* db) {
  sql::Statement s(db->GetUniqueStatement("PRAGMA page_size"));
  EXPECT_TRUE(s.Step());
  return s.ColumnInt(0);
}

// Get |name|'s root page number in the database.
int GetRootPage(sql::Connection* db, const char* name) {
  const char kPageSql[] = "SELECT rootpage FROM sqlite_master WHERE name = ?";
  sql::Statement s(db->GetUniqueStatement(kPageSql));
  s.BindString(0, name);
  EXPECT_TRUE(s.Step());
  return s.ColumnInt(0);
}

// Helper to read a SQLite page into a buffer.  |page_no| is 1-based
// per SQLite usage.
bool ReadPage(const base::FilePath& path, size_t page_no,
              char* buf, size_t page_size) {
  file_util::ScopedFILE file(file_util::OpenFile(path, "rb"));
  if (!file.get())
    return false;
  if (0 != fseek(file.get(), (page_no - 1) * page_size, SEEK_SET))
    return false;
  if (1u != fread(buf, page_size, 1, file.get()))
    return false;
  return true;
}

// Helper to write a SQLite page into a buffer.  |page_no| is 1-based
// per SQLite usage.
bool WritePage(const base::FilePath& path, size_t page_no,
               const char* buf, size_t page_size) {
  file_util::ScopedFILE file(file_util::OpenFile(path, "rb+"));
  if (!file.get())
    return false;
  if (0 != fseek(file.get(), (page_no - 1) * page_size, SEEK_SET))
    return false;
  if (1u != fwrite(buf, page_size, 1, file.get()))
    return false;
  return true;
}

// Verify that the up-to-date database has the expected tables and
// columns.  Functional tests only check whether the things which
// should be there are, but do not check if extraneous items are
// present.  Any extraneous items have the potential to interact
// negatively with future schema changes.
void VerifyTablesAndColumns(sql::Connection* db) {
  // [meta], [favicons], [favicon_bitmaps], and [icon_mapping].
  EXPECT_EQ(4u, sql::test::CountSQLTables(db));

  // Implicit index on [meta], index on [favicons], index on
  // [favicon_bitmaps], two indices on [icon_mapping].
  EXPECT_EQ(5u, sql::test::CountSQLIndices(db));

  // [key] and [value].
  EXPECT_EQ(2u, sql::test::CountTableColumns(db, "meta"));

  // [id], [url], and [icon_type].
  EXPECT_EQ(3u, sql::test::CountTableColumns(db, "favicons"));

  // [id], [icon_id], [last_updated], [image_data], [width], and [height].
  EXPECT_EQ(6u, sql::test::CountTableColumns(db, "favicon_bitmaps"));

  // [id], [page_url], and [icon_id].
  EXPECT_EQ(3u, sql::test::CountTableColumns(db, "icon_mapping"));
}

void VerifyDatabaseEmpty(sql::Connection* db) {
  size_t rows = 0;
  EXPECT_TRUE(sql::test::CountTableRows(db, "favicons", &rows));
  EXPECT_EQ(0u, rows);
  EXPECT_TRUE(sql::test::CountTableRows(db, "favicon_bitmaps", &rows));
  EXPECT_EQ(0u, rows);
  EXPECT_TRUE(sql::test::CountTableRows(db, "icon_mapping", &rows));
  EXPECT_EQ(0u, rows);
}

// Helper to check that an expected mapping exists.
WARN_UNUSED_RESULT bool CheckPageHasIcon(
    ThumbnailDatabase* db,
    const GURL& page_url,
    chrome::IconType expected_icon_type,
    const GURL& expected_icon_url,
    const gfx::Size& expected_icon_size,
    size_t expected_icon_contents_size,
    const unsigned char* expected_icon_contents) {
  std::vector<IconMapping> icon_mappings;
  if (!db->GetIconMappingsForPageURL(page_url, &icon_mappings)) {
    ADD_FAILURE() << "failed GetIconMappingsForPageURL()";
    return false;
  }

  // Scan for the expected type.
  std::vector<IconMapping>::const_iterator iter = icon_mappings.begin();
  for (; iter != icon_mappings.end(); ++iter) {
    if (iter->icon_type == expected_icon_type)
      break;
  }
  if (iter == icon_mappings.end()) {
    ADD_FAILURE() << "failed to find |expected_icon_type|";
    return false;
  }

  if (expected_icon_url != iter->icon_url) {
    EXPECT_EQ(expected_icon_url, iter->icon_url);
    return false;
  }

  std::vector<FaviconBitmap> favicon_bitmaps;
  if (!db->GetFaviconBitmaps(iter->icon_id, &favicon_bitmaps)) {
    ADD_FAILURE() << "failed GetFaviconBitmaps()";
    return false;
  }

  if (1 != favicon_bitmaps.size()) {
    EXPECT_EQ(1u, favicon_bitmaps.size());
    return false;
  }

  if (expected_icon_size != favicon_bitmaps[0].pixel_size) {
    EXPECT_EQ(expected_icon_size, favicon_bitmaps[0].pixel_size);
    return false;
  }

  if (expected_icon_contents_size != favicon_bitmaps[0].bitmap_data->size()) {
    EXPECT_EQ(expected_icon_contents_size,
              favicon_bitmaps[0].bitmap_data->size());
    return false;
  }

  if (memcmp(favicon_bitmaps[0].bitmap_data->front(),
             expected_icon_contents, expected_icon_contents_size)) {
    ADD_FAILURE() << "failed to match |expected_icon_contents|";
    return false;
  }
  return true;
}

}  // namespace

class ThumbnailDatabaseTest : public testing::Test {
 public:
  ThumbnailDatabaseTest() {
  }
  virtual ~ThumbnailDatabaseTest() {
  }

  // Initialize a thumbnail database instance from the SQL file at
  // |golden_path| in the "History/" subdirectory of test data.
  scoped_ptr<ThumbnailDatabase> LoadFromGolden(const char* golden_path) {
    if (!CreateDatabaseFromSQL(file_name_, golden_path)) {
      ADD_FAILURE() << "Failed loading " << golden_path;
      return scoped_ptr<ThumbnailDatabase>();
    }

    scoped_ptr<ThumbnailDatabase> db(new ThumbnailDatabase());
    EXPECT_EQ(sql::INIT_OK, db->Init(file_name_));
    db->BeginTransaction();

    return db.Pass();
  }

 protected:
  virtual void SetUp() {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.path().AppendASCII("TestFavicons.db");
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
};

TEST_F(ThumbnailDatabaseTest, AddIconMapping) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  base::Time time = base::Time::Now();
  chrome::FaviconID id = db.AddFavicon(url,
                                       chrome::TOUCH_ICON,
                                       favicon,
                                       time,
                                       gfx::Size());
  EXPECT_NE(0, id);

  EXPECT_NE(0, db.AddIconMapping(url, id));
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(url, icon_mappings.front().page_url);
  EXPECT_EQ(id, icon_mappings.front().icon_id);
}

TEST_F(ThumbnailDatabaseTest, UpdateIconMapping) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  GURL url("http://google.com");
  chrome::FaviconID id =
      db.AddFavicon(url, chrome::TOUCH_ICON);

  EXPECT_LT(0, db.AddIconMapping(url, id));
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  ASSERT_EQ(1u, icon_mapping.size());
  EXPECT_EQ(url, icon_mapping.front().page_url);
  EXPECT_EQ(id, icon_mapping.front().icon_id);

  GURL url1("http://www.google.com/");
  chrome::FaviconID new_id =
      db.AddFavicon(url1, chrome::TOUCH_ICON);
  EXPECT_TRUE(db.UpdateIconMapping(icon_mapping.front().mapping_id, new_id));

  icon_mapping.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  ASSERT_EQ(1u, icon_mapping.size());
  EXPECT_EQ(url, icon_mapping.front().page_url);
  EXPECT_EQ(new_id, icon_mapping.front().icon_id);
  EXPECT_NE(id, icon_mapping.front().icon_id);
}

TEST_F(ThumbnailDatabaseTest, DeleteIconMappings) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  chrome::FaviconID id =
      db.AddFavicon(url, chrome::TOUCH_ICON);
  base::Time time = base::Time::Now();
  db.AddFaviconBitmap(id, favicon, time, gfx::Size());
  EXPECT_LT(0, db.AddIconMapping(url, id));

  chrome::FaviconID id2 =
      db.AddFavicon(url, chrome::FAVICON);
  EXPECT_LT(0, db.AddIconMapping(url, id2));
  ASSERT_NE(id, id2);

  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  ASSERT_EQ(2u, icon_mapping.size());
  EXPECT_EQ(icon_mapping.front().icon_type, chrome::TOUCH_ICON);
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, chrome::FAVICON, NULL));

  db.DeleteIconMappings(url);

  EXPECT_FALSE(db.GetIconMappingsForPageURL(url, NULL));
  EXPECT_FALSE(db.GetIconMappingsForPageURL(url, chrome::FAVICON, NULL));
}

TEST_F(ThumbnailDatabaseTest, GetIconMappingsForPageURL) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");

  chrome::FaviconID id1 = db.AddFavicon(url, chrome::TOUCH_ICON);
  base::Time time = base::Time::Now();
  db.AddFaviconBitmap(id1, favicon, time, kSmallSize);
  db.AddFaviconBitmap(id1, favicon, time, kLargeSize);
  EXPECT_LT(0, db.AddIconMapping(url, id1));

  chrome::FaviconID id2 = db.AddFavicon(url, chrome::FAVICON);
  EXPECT_NE(id1, id2);
  db.AddFaviconBitmap(id2, favicon, time, kSmallSize);
  EXPECT_LT(0, db.AddIconMapping(url, id2));

  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mappings));
  ASSERT_EQ(2u, icon_mappings.size());
  EXPECT_EQ(id1, icon_mappings[0].icon_id);
  EXPECT_EQ(id2, icon_mappings[1].icon_id);
}

TEST_F(ThumbnailDatabaseTest, RetainDataForPageUrls) {
  ThumbnailDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL unkept_url("http://google.com/favicon2.ico");
  chrome::FaviconID unkept_id = db.AddFavicon(unkept_url, chrome::FAVICON);
  db.AddFaviconBitmap(unkept_id, favicon, base::Time::Now(), kSmallSize);

  GURL kept_url("http://google.com/favicon.ico");
  chrome::FaviconID kept_id = db.AddFavicon(kept_url, chrome::FAVICON);
  db.AddFaviconBitmap(kept_id, favicon, base::Time::Now(), kLargeSize);

  GURL unkept_page_url("http://chromium.org");
  db.AddIconMapping(unkept_page_url, unkept_id);
  db.AddIconMapping(unkept_page_url, kept_id);

  GURL kept_page_url("http://google.com");
  db.AddIconMapping(kept_page_url, kept_id);

  // RetainDataForPageUrls() uses schema manipulations for efficiency.
  // Grab a copy of the schema to make sure the final schema matches.
  const std::string original_schema = db.db_.GetSchema();

  EXPECT_TRUE(db.RetainDataForPageUrls(std::vector<GURL>(1, kept_page_url)));

  // Only copied data should be left.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(
                  kept_page_url, chrome::FAVICON, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(kept_page_url, icon_mappings[0].page_url);

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(db.GetFaviconBitmaps(icon_mappings[0].icon_id, &favicon_bitmaps));
  EXPECT_EQ(1u, favicon_bitmaps.size());
  EXPECT_EQ(kLargeSize, favicon_bitmaps[0].pixel_size);

  EXPECT_FALSE(db.GetFaviconIDForFaviconURL(unkept_url, false, NULL));

  // Schema should be the same.
  EXPECT_EQ(original_schema, db.db_.GetSchema());
}

// Tests that deleting a favicon deletes the favicon row and favicon bitmap
// rows from the database.
TEST_F(ThumbnailDatabaseTest, DeleteFavicon) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data1(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon1(
      new base::RefCountedBytes(data1));
  std::vector<unsigned char> data2(kBlob2, kBlob2 + sizeof(kBlob2));
  scoped_refptr<base::RefCountedBytes> favicon2(
      new base::RefCountedBytes(data2));

  GURL url("http://google.com");
  chrome::FaviconID id = db.AddFavicon(url, chrome::FAVICON);
  base::Time last_updated = base::Time::Now();
  db.AddFaviconBitmap(id, favicon1, last_updated, kSmallSize);
  db.AddFaviconBitmap(id, favicon2, last_updated, kLargeSize);

  EXPECT_TRUE(db.GetFaviconBitmaps(id, NULL));

  EXPECT_TRUE(db.DeleteFavicon(id));
  EXPECT_FALSE(db.GetFaviconBitmaps(id, NULL));
}

TEST_F(ThumbnailDatabaseTest, GetIconMappingsForPageURLForReturnOrder) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  // Add a favicon
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL page_url("http://google.com");
  GURL icon_url("http://google.com/favicon.ico");
  base::Time time = base::Time::Now();

  chrome::FaviconID id = db.AddFavicon(icon_url,
                                       chrome::FAVICON,
                                       favicon,
                                       time,
                                       gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id));
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id, icon_mappings.front().icon_id);
  EXPECT_EQ(chrome::FAVICON, icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);

  // Add a touch icon
  std::vector<unsigned char> data2(kBlob2, kBlob2 + sizeof(kBlob2));
  scoped_refptr<base::RefCountedBytes> favicon2 =
      new base::RefCountedBytes(data);

  chrome::FaviconID id2 = db.AddFavicon(icon_url,
                                        chrome::TOUCH_ICON,
                                        favicon2,
                                        time,
                                        gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id2));

  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id2, icon_mappings.front().icon_id);
  EXPECT_EQ(chrome::TOUCH_ICON, icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);

  // Add a touch precomposed icon
  scoped_refptr<base::RefCountedBytes> favicon3 =
      new base::RefCountedBytes(data2);

  chrome::FaviconID id3 = db.AddFavicon(icon_url,
                                        chrome::TOUCH_PRECOMPOSED_ICON,
                                        favicon3,
                                        time,
                                        gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id3));

  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id3, icon_mappings.front().icon_id);
  EXPECT_EQ(chrome::TOUCH_PRECOMPOSED_ICON, icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);
}

// Test result of GetIconMappingsForPageURL when an icon type is passed in.
TEST_F(ThumbnailDatabaseTest, GetIconMappingsForPageURLWithIconType) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  GURL url("http://google.com");
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));
  base::Time time = base::Time::Now();

  chrome::FaviconID id1 = db.AddFavicon(url,
                                        chrome::FAVICON,
                                        favicon,
                                        time,
                                        gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(url, id1));

  chrome::FaviconID id2 = db.AddFavicon(url,
                                        chrome::TOUCH_ICON,
                                        favicon,
                                        time,
                                        gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(url, id2));

  chrome::FaviconID id3 = db.AddFavicon(url,
                                        chrome::TOUCH_ICON,
                                        favicon,
                                        time,
                                        gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(url, id3));

  // Only the mappings for favicons of type TOUCH_ICON should be returned as
  // TOUCH_ICON is a larger icon type than FAVICON.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(
      url,
      chrome::FAVICON | chrome::TOUCH_ICON | chrome::TOUCH_PRECOMPOSED_ICON,
      &icon_mappings));

  EXPECT_EQ(2u, icon_mappings.size());
  if (id2 == icon_mappings[0].icon_id) {
    EXPECT_EQ(id3, icon_mappings[1].icon_id);
  } else {
    EXPECT_EQ(id3, icon_mappings[0].icon_id);
    EXPECT_EQ(id2, icon_mappings[1].icon_id);
  }

  icon_mappings.clear();
  EXPECT_TRUE(
      db.GetIconMappingsForPageURL(url, chrome::TOUCH_ICON, &icon_mappings));
  if (id2 == icon_mappings[0].icon_id) {
    EXPECT_EQ(id3, icon_mappings[1].icon_id);
  } else {
    EXPECT_EQ(id3, icon_mappings[0].icon_id);
    EXPECT_EQ(id2, icon_mappings[1].icon_id);
  }

  icon_mappings.clear();
  EXPECT_TRUE(
      db.GetIconMappingsForPageURL(url, chrome::FAVICON, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(id1, icon_mappings[0].icon_id);
}

TEST_F(ThumbnailDatabaseTest, HasMappingFor) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  // Add a favicon which will have icon_mappings
  base::Time time = base::Time::Now();
  chrome::FaviconID id1 = db.AddFavicon(GURL("http://google.com"),
                                        chrome::FAVICON,
                                        favicon,
                                        time,
                                        gfx::Size());
  EXPECT_NE(id1, 0);

  // Add another type of favicon
  time = base::Time::Now();
  chrome::FaviconID id2 = db.AddFavicon(GURL("http://www.google.com/icon"),
                                        chrome::TOUCH_ICON,
                                        favicon,
                                        time,
                                        gfx::Size());
  EXPECT_NE(id2, 0);

  // Add 3rd favicon
  time = base::Time::Now();
  chrome::FaviconID id3 = db.AddFavicon(GURL("http://www.google.com/icon"),
                                        chrome::TOUCH_ICON,
                                        favicon,
                                        time,
                                        gfx::Size());
  EXPECT_NE(id3, 0);

  // Add 2 icon mapping
  GURL page_url("http://www.google.com");
  EXPECT_TRUE(db.AddIconMapping(page_url, id1));
  EXPECT_TRUE(db.AddIconMapping(page_url, id2));

  EXPECT_TRUE(db.HasMappingFor(id1));
  EXPECT_TRUE(db.HasMappingFor(id2));
  EXPECT_FALSE(db.HasMappingFor(id3));

  // Remove all mappings
  db.DeleteIconMappings(page_url);
  EXPECT_FALSE(db.HasMappingFor(id1));
  EXPECT_FALSE(db.HasMappingFor(id2));
  EXPECT_FALSE(db.HasMappingFor(id3));
}

TEST_F(ThumbnailDatabaseTest, CloneIconMappings) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  // Add a favicon which will have icon_mappings
  chrome::FaviconID id1 = db.AddFavicon(
      GURL("http://google.com"), chrome::FAVICON);
  EXPECT_NE(0, id1);
  base::Time time = base::Time::Now();
  db.AddFaviconBitmap(id1, favicon, time, gfx::Size());

  // Add another type of favicon
  chrome::FaviconID id2 = db.AddFavicon(GURL("http://www.google.com/icon"),
                                        chrome::TOUCH_ICON);
  EXPECT_NE(0, id2);
  time = base::Time::Now();
  db.AddFaviconBitmap(id2, favicon, time, gfx::Size());

  // Add 3rd favicon
  chrome::FaviconID id3 = db.AddFavicon(GURL("http://www.google.com/icon"),
                                        chrome::TOUCH_ICON);
  EXPECT_NE(0, id3);
  time = base::Time::Now();
  db.AddFaviconBitmap(id3, favicon, time, gfx::Size());

  GURL page1_url("http://page1.com");
  EXPECT_TRUE(db.AddIconMapping(page1_url, id1));
  EXPECT_TRUE(db.AddIconMapping(page1_url, id2));

  GURL page2_url("http://page2.com");
  EXPECT_TRUE(db.AddIconMapping(page2_url, id3));

  // Test we do nothing with existing mappings.
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page2_url, &icon_mapping));
  ASSERT_EQ(1U, icon_mapping.size());

  EXPECT_TRUE(db.CloneIconMappings(page1_url, page2_url));

  icon_mapping.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page2_url, &icon_mapping));
  ASSERT_EQ(1U, icon_mapping.size());
  EXPECT_EQ(page2_url, icon_mapping[0].page_url);
  EXPECT_EQ(id3, icon_mapping[0].icon_id);

  // Test we clone if the new page has no mappings.
  GURL page3_url("http://page3.com");
  EXPECT_TRUE(db.CloneIconMappings(page1_url, page3_url));

  icon_mapping.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page3_url, &icon_mapping));

  ASSERT_EQ(2U, icon_mapping.size());
  if (icon_mapping[0].icon_id == id2)
    std::swap(icon_mapping[0], icon_mapping[1]);
  EXPECT_EQ(page3_url, icon_mapping[0].page_url);
  EXPECT_EQ(id1, icon_mapping[0].icon_id);
  EXPECT_EQ(page3_url, icon_mapping[1].page_url);
  EXPECT_EQ(id2, icon_mapping[1].icon_id);
}

// Test loading version 3 database.
TEST_F(ThumbnailDatabaseTest, Version3) {
  scoped_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v3.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  // Version 3 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 4 database.
TEST_F(ThumbnailDatabaseTest, Version4) {
  scoped_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v4.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  // Version 4 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 5 database.
TEST_F(ThumbnailDatabaseTest, Version5) {
  scoped_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v5.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl1, chrome::FAVICON,
                               kIconUrl1, gfx::Size(), sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl2, chrome::FAVICON,
                               kIconUrl2, gfx::Size(), sizeof(kBlob2), kBlob2));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3, chrome::FAVICON,
                               kIconUrl1, gfx::Size(), sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3, chrome::TOUCH_ICON,
                               kIconUrl3, gfx::Size(), sizeof(kBlob2), kBlob2));
}

// Test loading version 6 database.
TEST_F(ThumbnailDatabaseTest, Version6) {
  scoped_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v6.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl1, chrome::FAVICON,
                               kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl2, chrome::FAVICON,
                               kIconUrl2, kLargeSize, sizeof(kBlob2), kBlob2));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3, chrome::FAVICON,
                               kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3, chrome::TOUCH_ICON,
                               kIconUrl3, kLargeSize, sizeof(kBlob2), kBlob2));
}

// Test loading version 7 database.
TEST_F(ThumbnailDatabaseTest, Version7) {
  scoped_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v7.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl1, chrome::FAVICON,
                               kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl2, chrome::FAVICON,
                               kIconUrl2, kLargeSize, sizeof(kBlob2), kBlob2));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3, chrome::FAVICON,
                               kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3, chrome::TOUCH_ICON,
                               kIconUrl3, kLargeSize, sizeof(kBlob2), kBlob2));
}

TEST_F(ThumbnailDatabaseTest, Recovery) {
  // This code tests the recovery module in concert with Chromium's
  // custom recover virtual table.  Under USE_SYSTEM_SQLITE, this is
  // not available.  This is detected dynamically because corrupt
  // databases still need to be handled, perhaps by Raze(), and the
  // recovery module is an obvious layer to abstract that to.
  // TODO(shess): Handle that case for real!
  if (!sql::Recovery::FullRecoverySupported())
    return;

  // Create an example database.
  {
    EXPECT_TRUE(CreateDatabaseFromSQL(file_name_, "Favicons.v7.sql"));

    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    VerifyTablesAndColumns(&raw_db);
  }

  // Test that the contents make sense after clean open.
  {
    ThumbnailDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    EXPECT_TRUE(
        CheckPageHasIcon(&db, kPageUrl1, chrome::FAVICON,
                         kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
    EXPECT_TRUE(
        CheckPageHasIcon(&db, kPageUrl2, chrome::FAVICON,
                         kIconUrl2, kLargeSize, sizeof(kBlob2), kBlob2));
  }

  // Corrupt the |icon_mapping.page_url| index by deleting an element
  // from the backing table but not the index.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    {
      sql::Statement statement(
          raw_db.GetUniqueStatement("PRAGMA integrity_check"));
      EXPECT_TRUE(statement.Step());
      ASSERT_EQ("ok", statement.ColumnString(0));
    }

    const char kIndexName[] = "icon_mapping_page_url_idx";
    const int idx_root_page = GetRootPage(&raw_db, kIndexName);
    const int page_size = GetPageSize(&raw_db);
    scoped_ptr<char[]> buf(new char[page_size]);
    EXPECT_TRUE(ReadPage(file_name_, idx_root_page, buf.get(), page_size));

    {
      const char kDeleteSql[] = "DELETE FROM icon_mapping WHERE page_url = ?";
      sql::Statement statement(raw_db.GetUniqueStatement(kDeleteSql));
      statement.BindString(0, URLDatabase::GURLToDatabaseURL(kPageUrl2));
      EXPECT_TRUE(statement.Run());
    }
    raw_db.Close();

    EXPECT_TRUE(WritePage(file_name_, idx_root_page, buf.get(), page_size));
  }

  // Database should be corrupt at the SQLite level.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    sql::Statement statement(
        raw_db.GetUniqueStatement("PRAGMA integrity_check"));
    EXPECT_TRUE(statement.Step());
    ASSERT_NE("ok", statement.ColumnString(0));
  }

  // Open the database and access the corrupt index.
  {
    sql::ScopedErrorIgnorer ignore_errors;
    ignore_errors.IgnoreError(SQLITE_CORRUPT);
    ThumbnailDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    // Data for kPageUrl2 was deleted, but the index entry remains,
    // this will throw SQLITE_CORRUPT.  The corruption handler will
    // recover the database and poison the handle, so the outer call
    // fails.
    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));

    ASSERT_TRUE(ignore_errors.CheckIgnoredErrors());
  }

  // Check that the database is recovered at the SQLite level.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    sql::Statement statement(
        raw_db.GetUniqueStatement("PRAGMA integrity_check"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ("ok", statement.ColumnString(0));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);
  }

  // Database should also be recovered at higher levels.
  {
    ThumbnailDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    // Now this fails because there is no mapping.
    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));

    // Other data was retained by recovery.
    EXPECT_TRUE(
        CheckPageHasIcon(&db, kPageUrl1, chrome::FAVICON,
                         kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
  }

  // Corrupt the database again by making the actual file shorter than
  // the header expects.
  {
    int64 db_size = 0;
    EXPECT_TRUE(file_util::GetFileSize(file_name_, &db_size));
    {
      sql::Connection raw_db;
      EXPECT_TRUE(raw_db.Open(file_name_));
      EXPECT_TRUE(raw_db.Execute("CREATE TABLE t(x)"));
    }
    file_util::ScopedFILE file(file_util::OpenFile(file_name_, "rb+"));
    ASSERT_TRUE(file.get() != NULL);
    EXPECT_EQ(0, fseek(file.get(), static_cast<long>(db_size), SEEK_SET));
    EXPECT_TRUE(file_util::TruncateFile(file.get()));
  }

  // Database is unusable at the SQLite level.
  {
    sql::ScopedErrorIgnorer ignore_errors;
    ignore_errors.IgnoreError(SQLITE_CORRUPT);
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    EXPECT_FALSE(raw_db.IsSQLValid("PRAGMA integrity_check"));
    ASSERT_TRUE(ignore_errors.CheckIgnoredErrors());
  }

  // Database should be recovered during open.
  {
    sql::ScopedErrorIgnorer ignore_errors;
    ignore_errors.IgnoreError(SQLITE_CORRUPT);
    ThumbnailDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));
    EXPECT_TRUE(
        CheckPageHasIcon(&db, kPageUrl1, chrome::FAVICON,
                         kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));

    ASSERT_TRUE(ignore_errors.CheckIgnoredErrors());
  }
}

TEST_F(ThumbnailDatabaseTest, Recovery6) {
  // TODO(shess): See comment at top of Recovery test.
  if (!sql::Recovery::FullRecoverySupported())
    return;

  // Create an example database without loading into ThumbnailDatabase
  // (which would upgrade it).
  EXPECT_TRUE(CreateDatabaseFromSQL(file_name_, "Favicons.v6.sql"));

  // Corrupt the database by making the actual file shorter than the
  // SQLite header expects.  This form of corruption will cause
  // immediate failures during Open(), before the migration code runs,
  // so the version-6 recovery will occur.
  {
    int64 db_size = 0;
    EXPECT_TRUE(file_util::GetFileSize(file_name_, &db_size));
    {
      sql::Connection raw_db;
      EXPECT_TRUE(raw_db.Open(file_name_));
      EXPECT_TRUE(raw_db.Execute("CREATE TABLE t(x)"));
    }
    file_util::ScopedFILE file(file_util::OpenFile(file_name_, "rb+"));
    ASSERT_TRUE(file.get() != NULL);
    EXPECT_EQ(0, fseek(file.get(), static_cast<long>(db_size), SEEK_SET));
    EXPECT_TRUE(file_util::TruncateFile(file.get()));
  }

  // Database is unusable at the SQLite level.
  {
    sql::ScopedErrorIgnorer ignore_errors;
    ignore_errors.IgnoreError(SQLITE_CORRUPT);
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    EXPECT_FALSE(raw_db.IsSQLValid("PRAGMA integrity_check"));
    ASSERT_TRUE(ignore_errors.CheckIgnoredErrors());
  }

  // Database should be recovered during open.
  {
    sql::ScopedErrorIgnorer ignore_errors;
    ignore_errors.IgnoreError(SQLITE_CORRUPT);
    ThumbnailDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    // Test that some data is present, copied from
    // ThumbnailDatabaseTest.Version6 .
    EXPECT_TRUE(
        CheckPageHasIcon(&db, kPageUrl3, chrome::FAVICON,
                         kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
    EXPECT_TRUE(
        CheckPageHasIcon(&db, kPageUrl3, chrome::TOUCH_ICON,
                         kIconUrl3, kLargeSize, sizeof(kBlob2), kBlob2));

    ASSERT_TRUE(ignore_errors.CheckIgnoredErrors());
  }

  // Check that the database is recovered at a SQLite level, and that
  // the current schema is in place.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    sql::Statement statement(
        raw_db.GetUniqueStatement("PRAGMA integrity_check"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ("ok", statement.ColumnString(0));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);
  }
}

}  // namespace history
