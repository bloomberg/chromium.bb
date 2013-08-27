// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_unittest_base.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

// data we'll put into the thumbnail database
static const unsigned char blob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";
static const unsigned char blob2[] =
    "goiwuegrqrcomizqyzkjalitbahxfjytrqvpqeroicxmnlkhlzunacxaneviawrtxcywhgef";
static const unsigned char blob3[] =
    "3716871354098370776510470746794707624107647054607467847164027";
const double kBoringness = 0.25;
const double kWorseBoringness = 0.50;
const double kBetterBoringness = 0.10;
const double kTotallyBoring = 1.0;

const int64 kPage1 = 1234;

const gfx::Size kSmallSize = gfx::Size(16, 16);
const gfx::Size kLargeSize = gfx::Size(32, 32);

}  // namespace

class ThumbnailDatabaseTest : public testing::Test {
 public:
  ThumbnailDatabaseTest() {
  }
  virtual ~ThumbnailDatabaseTest() {
  }

 protected:
  virtual void SetUp() {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.path().AppendASCII("TestThumbnails.db");
    new_file_name_ = temp_dir_.path().AppendASCII("TestFavicons.db");
    history_db_name_ = temp_dir_.path().AppendASCII("TestHistory.db");
    google_bitmap_.reset(
        gfx::JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  }

  scoped_ptr<SkBitmap> google_bitmap_;

  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
  base::FilePath new_file_name_;
  base::FilePath history_db_name_;
};

class IconMappingMigrationTest : public HistoryUnitTestBase {
 public:
  IconMappingMigrationTest() {
  }
  virtual ~IconMappingMigrationTest() {
  }

 protected:
  virtual void SetUp() {
    profile_.reset(new TestingProfile);

    base::FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("History");

    history_db_name_ = profile_->GetPath().Append(chrome::kHistoryFilename);
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQLScript(data_path.AppendASCII("history.20.sql"),
                         history_db_name_));
    thumbnail_db_name_ =
        profile_->GetPath().Append(chrome::kFaviconsFilename);
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQLScript(data_path.AppendASCII("thumbnails.3.sql"),
                         thumbnail_db_name_));
  }

 protected:
  base::FilePath history_db_name_;
  base::FilePath thumbnail_db_name_;

 private:
  scoped_ptr<TestingProfile> profile_;
};

TEST_F(ThumbnailDatabaseTest, AddIconMapping) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
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
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
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
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
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
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
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

// Test upgrading database to version 4.
TEST_F(ThumbnailDatabaseTest, UpgradeToVersion4) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  const char* name = "favicons";
  std::string sql;
  sql.append("DROP TABLE IF EXISTS ");
  sql.append(name);
  EXPECT_TRUE(db.db_.Execute(sql.c_str()));

  sql.resize(0);
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append("("
             "id INTEGER PRIMARY KEY,"
             "url LONGVARCHAR NOT NULL,"
             "last_updated INTEGER DEFAULT 0,"
             "image_data BLOB)");
  EXPECT_TRUE(db.db_.Execute(sql.c_str()));

  EXPECT_TRUE(db.UpgradeToVersion4());

  GURL url("http://google.com");

  sql::Statement statement;
  statement.Assign(db.db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO favicons (url, icon_type) VALUES (?, ?)"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(url));
  statement.BindInt(1, chrome::TOUCH_ICON);
  EXPECT_TRUE(statement.Run());

  statement.Assign(db.db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT icon_type FROM favicons"));
  EXPECT_TRUE(statement.Step());

  EXPECT_EQ(chrome::TOUCH_ICON,
            static_cast<chrome::IconType>(statement.ColumnInt(0)));
}

// Test upgrading database to version 5.
TEST_F(ThumbnailDatabaseTest, UpgradeToVersion5) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  const char* name = "favicons";
  std::string sql;
  sql.append("DROP TABLE IF EXISTS ");
  sql.append(name);
  EXPECT_TRUE(db.db_.Execute(sql.c_str()));

  sql.resize(0);
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append("("
             "id INTEGER PRIMARY KEY,"
             "url LONGVARCHAR NOT NULL,"
             "last_updated INTEGER DEFAULT 0,"
             "image_data BLOB,"
             "icon_type INTEGER DEFAULT 1)");
  ASSERT_TRUE(db.db_.Execute(sql.c_str()));

  ASSERT_TRUE(db.UpgradeToVersion5());

  sql = "SELECT sizes FROM favicons";
  EXPECT_TRUE(db.db_.Execute(sql.c_str()));
}

// Test upgrading database to version 6.
TEST_F(ThumbnailDatabaseTest, UpgradeToVersion6) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  const char* name = "favicons";
  std::string sql;
  sql.append("DROP TABLE IF EXISTS ");
  sql.append(name);
  EXPECT_TRUE(db.db_.Execute(sql.c_str()));

  sql.clear();
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append("("
             "id INTEGER PRIMARY KEY,"
             "url LONGVARCHAR NOT NULL,"
             "last_updated INTEGER DEFAULT 0,"
             "image_data BLOB,"
             "icon_type INTEGER DEFAULT 1,"
             "sizes LONGVARCHAR)");
  EXPECT_TRUE(db.db_.Execute(sql.c_str()));

  int favicon_id = 1;
  GURL url("http://google.com");
  int64 last_updated = Time::Now().ToInternalValue();
  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));

  sql::Statement statement;
  statement.Assign(db.db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO favicons (id, url, last_updated, image_data, icon_type, "
      "sizes) VALUES (?, ?, ?, ?, ?, ?)"));
  statement.BindInt(0, favicon_id);
  statement.BindString(1, URLDatabase::GURLToDatabaseURL(url));
  statement.BindInt64(2, last_updated);
  statement.BindBlob(3, bitmap_data->front(),
                     static_cast<int>(bitmap_data->size()));
  statement.BindInt(4, chrome::TOUCH_ICON);
  statement.BindCString(5, "Data which happened to be there");
  EXPECT_TRUE(statement.Run());

  EXPECT_TRUE(db.UpgradeToVersion6());

  statement.Assign(db.db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, url, icon_type, sizes FROM favicons"));
  EXPECT_TRUE(statement.Step());
  EXPECT_EQ(favicon_id, statement.ColumnInt(0));
  EXPECT_EQ(url, GURL(statement.ColumnString(1)));
  EXPECT_EQ(chrome::TOUCH_ICON, statement.ColumnInt(2));
  // Any previous data in sizes should be cleared.
  EXPECT_EQ(std::string(), statement.ColumnString(3));

  statement.Assign(db.db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT icon_id, last_updated, image_data, width, height "
      "FROM favicon_bitmaps"));
  EXPECT_TRUE(statement.Step());
  EXPECT_EQ(favicon_id, statement.ColumnInt(0));
  EXPECT_EQ(last_updated, statement.ColumnInt64(1));
  EXPECT_EQ(static_cast<int>(bitmap_data->size()),
            statement.ColumnByteLength(2));
  EXPECT_EQ(0, statement.ColumnInt(3));
  EXPECT_EQ(0, statement.ColumnInt(4));
}

// Test upgrading database to version 7.
TEST_F(ThumbnailDatabaseTest, UpgradeToVersion7) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  const char* name = "favicons";
  std::string sql;
  sql.append("DROP TABLE IF EXISTS ");
  sql.append(name);
  EXPECT_TRUE(db.db_.Execute(sql.c_str()));

  sql.clear();
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append("("
             "id INTEGER PRIMARY KEY,"
             "url LONGVARCHAR NOT NULL,"
             "icon_type INTEGER DEFAULT 1,"
             "sizes LONGVARCHAR)");
  EXPECT_TRUE(db.db_.Execute(sql.c_str()));

  int favicon_id = 1;
  GURL url("http://google.com");

  sql::Statement statement;
  statement.Assign(db.db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO favicons (id, url, icon_type, sizes) "
      "VALUES (?, ?, ?, ?)"));
  statement.BindInt(0, favicon_id);
  statement.BindString(1, URLDatabase::GURLToDatabaseURL(url));
  statement.BindInt(2, chrome::TOUCH_ICON);
  statement.BindCString(3, "Data which happened to be there");
  EXPECT_TRUE(statement.Run());

  EXPECT_TRUE(db.UpgradeToVersion7());

  EXPECT_FALSE(db.db_.DoesColumnExist("favicons", "sizes"));

  statement.Assign(db.db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, url, icon_type FROM favicons"));
  EXPECT_TRUE(statement.Step());
  EXPECT_EQ(favicon_id, statement.ColumnInt(0));
  EXPECT_EQ(url, GURL(statement.ColumnString(1)));
  EXPECT_EQ(chrome::TOUCH_ICON, statement.ColumnInt(2));
}

TEST_F(ThumbnailDatabaseTest, RetainDataForPageUrls) {
  ThumbnailDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));

  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
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
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data1(blob1, blob1 + sizeof(blob1));
  scoped_refptr<base::RefCountedBytes> favicon1(
      new base::RefCountedBytes(data1));
  std::vector<unsigned char> data2(blob2, blob2 + sizeof(blob2));
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
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  // Add a favicon
  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
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
  std::vector<unsigned char> data2(blob2, blob2 + sizeof(blob2));
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
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  GURL url("http://google.com");
  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
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
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
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
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
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

TEST_F(IconMappingMigrationTest, TestIconMappingMigration) {
  HistoryDatabase history_db;
  ASSERT_TRUE(history_db.db_.Open(history_db_name_));
  history_db.BeginTransaction();

  const GURL icon1 = GURL("http://www.google.com/favicon.ico");
  const GURL icon2 = GURL("http://www.yahoo.com/favicon.ico");

  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(thumbnail_db_name_, NULL, &history_db));
  db.BeginTransaction();

  // Migration should be done.
  // Test one icon_mapping.
  GURL page_url1 = GURL("http://google.com/");
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url1, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(chrome::FAVICON, icon_mappings[0].icon_type);
  EXPECT_EQ(page_url1, icon_mappings[0].page_url);
  EXPECT_EQ(1, icon_mappings[0].icon_id);
  EXPECT_EQ(icon1, icon_mappings[0].icon_url);

  // Test a page which has the same icon.
  GURL page_url3 = GURL("http://www.google.com/");
  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url3, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(chrome::FAVICON, icon_mappings[0].icon_type);
  EXPECT_EQ(page_url3, icon_mappings[0].page_url);
  EXPECT_EQ(1, icon_mappings[0].icon_id);
  EXPECT_EQ(icon1, icon_mappings[0].icon_url);

  // Test a icon_mapping with different IconID.
  GURL page_url2 = GURL("http://yahoo.com/");
  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url2, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(chrome::FAVICON, icon_mappings[0].icon_type);
  EXPECT_EQ(page_url2, icon_mappings[0].page_url);
  EXPECT_EQ(2, icon_mappings[0].icon_id);
  EXPECT_EQ(icon2, icon_mappings[0].icon_url);

  // Test a page without icon
  GURL page_url4 = GURL("http://www.google.com/blank.html");
  EXPECT_FALSE(db.GetIconMappingsForPageURL(page_url4, NULL));
}

TEST_F(ThumbnailDatabaseTest, IconMappingEnumerator) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  GURL icon_url1("http://google.com/favicon.ico");
  chrome::FaviconID touch_icon_id1 = db.AddFavicon(icon_url1,
                                                   chrome::TOUCH_ICON,
                                                   favicon,
                                                   base::Time::Now(),
                                                   gfx::Size());
  ASSERT_NE(0, touch_icon_id1);
  IconMappingID touch_mapping_id1 = db.AddIconMapping(url, touch_icon_id1);
  ASSERT_NE(0, touch_mapping_id1);

  chrome::FaviconID favicon_id1 = db.AddFavicon(icon_url1,
                                                chrome::FAVICON,
                                                favicon,
                                                base::Time::Now(),
                                                gfx::Size());
  ASSERT_NE(0, favicon_id1);
  IconMappingID favicon_mapping_id1 = db.AddIconMapping(url, favicon_id1);
  ASSERT_NE(0, favicon_mapping_id1);

  GURL url2("http://chromium.org");
  GURL icon_url2("http://chromium.org/favicon.ico");
  chrome::FaviconID favicon_id2 = db.AddFavicon(icon_url2,
                                                chrome::FAVICON,
                                                favicon,
                                                base::Time::Now(),
                                                gfx::Size());
  ASSERT_NE(0, favicon_id2);
  IconMappingID favicon_mapping_id2 = db.AddIconMapping(url2, favicon_id2);
  ASSERT_NE(0, favicon_mapping_id2);

  IconMapping icon_mapping;
  ThumbnailDatabase::IconMappingEnumerator enumerator1;
  ASSERT_TRUE(db.InitIconMappingEnumerator(chrome::FAVICON, &enumerator1));
  // There are 2 favicon mappings.
  bool has_favicon_mapping1 = false;
  bool has_favicon_mapping2 = false;
  int mapping_count = 0;
  while (enumerator1.GetNextIconMapping(&icon_mapping)) {
    mapping_count++;
    if (favicon_mapping_id1 == icon_mapping.mapping_id) {
      has_favicon_mapping1 = true;
      EXPECT_EQ(url, icon_mapping.page_url);
      EXPECT_EQ(favicon_id1, icon_mapping.icon_id);
      EXPECT_EQ(icon_url1, icon_mapping.icon_url);
      EXPECT_EQ(chrome::FAVICON, icon_mapping.icon_type);
    } else if (favicon_mapping_id2 == icon_mapping.mapping_id) {
      has_favicon_mapping2 = true;
      EXPECT_EQ(url2, icon_mapping.page_url);
      EXPECT_EQ(favicon_id2, icon_mapping.icon_id);
      EXPECT_EQ(icon_url2, icon_mapping.icon_url);
      EXPECT_EQ(chrome::FAVICON, icon_mapping.icon_type);
    }
  }
  EXPECT_EQ(2, mapping_count);
  EXPECT_TRUE(has_favicon_mapping1);
  EXPECT_TRUE(has_favicon_mapping2);

  ThumbnailDatabase::IconMappingEnumerator enumerator2;
  ASSERT_TRUE(db.InitIconMappingEnumerator(chrome::TOUCH_ICON, &enumerator2));
  ASSERT_TRUE(enumerator2.GetNextIconMapping(&icon_mapping));
  EXPECT_EQ(touch_mapping_id1, icon_mapping.mapping_id);
  EXPECT_EQ(url, icon_mapping.page_url);
  EXPECT_EQ(touch_icon_id1, icon_mapping.icon_id);
  EXPECT_EQ(icon_url1, icon_mapping.icon_url);
  EXPECT_EQ(chrome::TOUCH_ICON, icon_mapping.icon_type);

  EXPECT_FALSE(enumerator2.GetNextIconMapping(&icon_mapping));
}

}  // namespace history
