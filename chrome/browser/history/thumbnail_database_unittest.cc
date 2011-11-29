// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_unittest_base.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

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

}  // namespace

class ThumbnailDatabaseTest : public testing::Test {
 public:
  ThumbnailDatabaseTest() {
  }
  ~ThumbnailDatabaseTest() {
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

  ScopedTempDir temp_dir_;
  FilePath file_name_;
  FilePath new_file_name_;
  FilePath history_db_name_;
};

class IconMappingMigrationTest : public HistoryUnitTestBase {
 public:
  IconMappingMigrationTest() {
  }
  ~IconMappingMigrationTest() {
  }

 protected:
  virtual void SetUp() {
    profile_.reset(new TestingProfile);

    FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("History");

    history_db_name_ = profile_->GetPath().Append(chrome::kHistoryFilename);
    // Set up history and thumbnails as they would be before migration.
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQLScript(data_path.AppendASCII("history.20.sql"),
                         history_db_name_));
    thumbnail_db_name_ =
        profile_->GetPath().Append(chrome::kThumbnailsFilename);
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQLScript(data_path.AppendASCII("thumbnails.3.sql"),
                         thumbnail_db_name_));
  }

 protected:
  FilePath history_db_name_;
  FilePath thumbnail_db_name_;

 private:
  scoped_ptr<TestingProfile> profile_;
};

TEST_F(ThumbnailDatabaseTest, GetFaviconAfterMigrationToTopSites) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");
  FaviconID id = db.AddFavicon(url, FAVICON);
  base::Time time = base::Time::Now();
  db.SetFavicon(id, favicon, time);
  EXPECT_TRUE(db.RenameAndDropThumbnails(file_name_, new_file_name_));
  EXPECT_TRUE(db.IsLatestVersion());

  base::Time time_out;
  std::vector<unsigned char> favicon_out;
  GURL url_out;
  EXPECT_TRUE(db.GetFavicon(id, &time_out, &favicon_out, &url_out));
  EXPECT_EQ(url, url_out);
  EXPECT_EQ(time.ToTimeT(), time_out.ToTimeT());
  ASSERT_EQ(data.size(), favicon_out.size());
  EXPECT_TRUE(std::equal(data.begin(),
                         data.end(),
                         favicon_out.begin()));
}

TEST_F(ThumbnailDatabaseTest, AddIconMapping) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");
  FaviconID id = db.AddFavicon(url, TOUCH_ICON);
  EXPECT_NE(0, id);
  base::Time time = base::Time::Now();
  db.SetFavicon(id, favicon, time);

  EXPECT_NE(0, db.AddIconMapping(url, id));
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  EXPECT_EQ(1u, icon_mapping.size());
  EXPECT_EQ(url, icon_mapping.front().page_url);
  EXPECT_EQ(id, icon_mapping.front().icon_id);
}

TEST_F(ThumbnailDatabaseTest, UpdateIconMapping) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");
  FaviconID id = db.AddFavicon(url, TOUCH_ICON);
  base::Time time = base::Time::Now();
  db.SetFavicon(id, favicon, time);

  EXPECT_TRUE(0 < db.AddIconMapping(url, id));
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  ASSERT_EQ(1u, icon_mapping.size());
  EXPECT_EQ(url, icon_mapping.front().page_url);
  EXPECT_EQ(id, icon_mapping.front().icon_id);

  GURL url1("http://www.google.com/");
  FaviconID new_id = db.AddFavicon(url1, TOUCH_ICON);
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
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");
  FaviconID id = db.AddFavicon(url, TOUCH_ICON);
  base::Time time = base::Time::Now();
  db.SetFavicon(id, favicon, time);
  EXPECT_TRUE(0 < db.AddIconMapping(url, id));

  FaviconID id2 = db.AddFavicon(url, FAVICON);
  db.SetFavicon(id2, favicon, time);
  EXPECT_TRUE(0 < db.AddIconMapping(url, id2));
  ASSERT_NE(id, id2);

  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  ASSERT_EQ(2u, icon_mapping.size());
  EXPECT_EQ(icon_mapping.front().icon_type, TOUCH_ICON);
  EXPECT_TRUE(db.GetIconMappingForPageURL(url, FAVICON, NULL));

  db.DeleteIconMappings(url);

  EXPECT_FALSE(db.GetIconMappingsForPageURL(url, NULL));
  EXPECT_FALSE(db.GetIconMappingForPageURL(url, FAVICON, NULL));
}

TEST_F(ThumbnailDatabaseTest, GetIconMappingsForPageURL) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");

  FaviconID id1 = db.AddFavicon(url, TOUCH_ICON);
  base::Time time = base::Time::Now();
  db.SetFavicon(id1, favicon, time);
  EXPECT_TRUE(0 < db.AddIconMapping(url, id1));

  FaviconID id2 = db.AddFavicon(url, FAVICON);
  EXPECT_NE(id1, id2);
  db.SetFavicon(id2, favicon, time);
  EXPECT_TRUE(0 < db.AddIconMapping(url, id2));

  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  ASSERT_EQ(2u, icon_mapping.size());
  EXPECT_NE(icon_mapping[0].icon_id, icon_mapping[1].icon_id);
  EXPECT_TRUE(icon_mapping[0].icon_id == id1 && icon_mapping[1].icon_id == id2);
}

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

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");
  FaviconID id = db.AddFavicon(url, TOUCH_ICON);
  base::Time time = base::Time::Now();
  db.SetFavicon(id, favicon, time);

  EXPECT_TRUE(0 < db.AddIconMapping(url, id));
  IconMapping icon_mapping;
  EXPECT_TRUE(db.GetIconMappingForPageURL(url, TOUCH_ICON, &icon_mapping));
  EXPECT_EQ(url, icon_mapping.page_url);
  EXPECT_EQ(id, icon_mapping.icon_id);
}

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

TEST_F(ThumbnailDatabaseTest, TemporayIconMapping) {
  ThumbnailDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));

  db.BeginTransaction();

  EXPECT_TRUE(db.InitTemporaryIconMappingTable());

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");
  FaviconID id = db.AddFavicon(url, FAVICON);
  base::Time time = base::Time::Now();
  db.SetFavicon(id, favicon, time);

  db.AddToTemporaryIconMappingTable(url, id);
  db.CommitTemporaryIconMappingTable();
  IconMapping icon_mapping;
  EXPECT_TRUE(db.GetIconMappingForPageURL(url, FAVICON, &icon_mapping));
  EXPECT_EQ(id, icon_mapping.icon_id);
  EXPECT_EQ(url, icon_mapping.page_url);
}

TEST_F(ThumbnailDatabaseTest, GetIconMappingsForPageURLForReturnOrder) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  // Add a favicon
  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  GURL url("http://google.com");
  FaviconID id = db.AddFavicon(url, FAVICON);
  base::Time time = base::Time::Now();
  db.SetFavicon(id, favicon, time);

  EXPECT_NE(0, db.AddIconMapping(url, id));
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));

  EXPECT_EQ(url, icon_mapping.front().page_url);
  EXPECT_EQ(id, icon_mapping.front().icon_id);
  EXPECT_EQ(FAVICON, icon_mapping.front().icon_type);

  // Add a touch icon
  std::vector<unsigned char> data2(blob2, blob2 + sizeof(blob2));
  scoped_refptr<RefCountedBytes> favicon2(new RefCountedBytes(data));

  FaviconID id2 = db.AddFavicon(url, TOUCH_ICON);
  db.SetFavicon(id2, favicon2, time);
  EXPECT_NE(0, db.AddIconMapping(url, id2));

  icon_mapping.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));

  EXPECT_EQ(url, icon_mapping.front().page_url);
  EXPECT_EQ(id2, icon_mapping.front().icon_id);
  EXPECT_EQ(TOUCH_ICON, icon_mapping.front().icon_type);

  // Add a touch precomposed icon
  scoped_refptr<RefCountedBytes> favicon3(new RefCountedBytes(data2));

  FaviconID id3 = db.AddFavicon(url, TOUCH_PRECOMPOSED_ICON);
  db.SetFavicon(id3, favicon3, time);
  EXPECT_NE(0, db.AddIconMapping(url, id3));

  icon_mapping.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));

  EXPECT_EQ(url, icon_mapping.front().page_url);
  EXPECT_EQ(id3, icon_mapping.front().icon_id);
  EXPECT_EQ(TOUCH_PRECOMPOSED_ICON, icon_mapping.front().icon_type);
}

TEST_F(ThumbnailDatabaseTest, HasMappingFor) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  // Add a favicon which will have icon_mappings
  FaviconID id1 = db.AddFavicon(GURL("http://google.com"), FAVICON);
  EXPECT_NE(id1, 0);
  base::Time time = base::Time::Now();
  db.SetFavicon(id1, favicon, time);

  // Add another type of favicon
  FaviconID id2 = db.AddFavicon(GURL("http://www.google.com/icon"), TOUCH_ICON);
  EXPECT_NE(id2, 0);
  time = base::Time::Now();
  db.SetFavicon(id2, favicon, time);

  // Add 3rd favicon
  FaviconID id3 = db.AddFavicon(GURL("http://www.google.com/icon"), TOUCH_ICON);
  EXPECT_NE(id3, 0);
  time = base::Time::Now();
  db.SetFavicon(id3, favicon, time);

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

TEST_F(ThumbnailDatabaseTest, CloneIconMapping) {
  ThumbnailDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_, NULL, NULL));
  db.BeginTransaction();

  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> favicon(new RefCountedBytes(data));

  // Add a favicon which will have icon_mappings
  FaviconID id1 = db.AddFavicon(GURL("http://google.com"), FAVICON);
  EXPECT_NE(0, id1);
  base::Time time = base::Time::Now();
  db.SetFavicon(id1, favicon, time);

  // Add another type of favicon
  FaviconID id2 = db.AddFavicon(GURL("http://www.google.com/icon"), TOUCH_ICON);
  EXPECT_NE(0, id2);
  time = base::Time::Now();
  db.SetFavicon(id2, favicon, time);

  // Add 3rd favicon
  FaviconID id3 = db.AddFavicon(GURL("http://www.google.com/icon"), TOUCH_ICON);
  EXPECT_NE(0, id3);
  time = base::Time::Now();
  db.SetFavicon(id3, favicon, time);

  GURL page1_url("http://page1.com");
  EXPECT_TRUE(db.AddIconMapping(page1_url, id1));
  EXPECT_TRUE(db.AddIconMapping(page1_url, id2));

  GURL page2_url("http://page2.com");
  EXPECT_TRUE(db.AddIconMapping(page2_url, id3));

  // Test we do nothing with existing mappings.
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page2_url, &icon_mapping));
  ASSERT_EQ(1U, icon_mapping.size());

  EXPECT_TRUE(db.CloneIconMapping(page1_url, page2_url));

  icon_mapping.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page2_url, &icon_mapping));
  ASSERT_EQ(1U, icon_mapping.size());
  EXPECT_EQ(page2_url, icon_mapping[0].page_url);
  EXPECT_EQ(id3, icon_mapping[0].icon_id);

  // Test we clone if the new page has no mappings.
  GURL page3_url("http://page3.com");
  EXPECT_TRUE(db.CloneIconMapping(page1_url, page3_url));

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
  EXPECT_EQ(FAVICON, icon_mappings[0].icon_type);
  EXPECT_EQ(page_url1, icon_mappings[0].page_url);
  EXPECT_EQ(1, icon_mappings[0].icon_id);
  base::Time time;
  std::vector<unsigned char> out_data;
  GURL out_icon_url;
  ASSERT_TRUE(db.GetFavicon(
      icon_mappings[0].icon_id, &time, &out_data, &out_icon_url));
  EXPECT_EQ(icon1, out_icon_url);

  // Test a page which has the same icon.
  GURL page_url3 = GURL("http://www.google.com/");
  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url3, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(FAVICON, icon_mappings[0].icon_type);
  EXPECT_EQ(page_url3, icon_mappings[0].page_url);
  EXPECT_EQ(1, icon_mappings[0].icon_id);

  // Test a icon_mapping with different IconID.
  GURL page_url2 = GURL("http://yahoo.com/");
  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url2, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(FAVICON, icon_mappings[0].icon_type);
  EXPECT_EQ(page_url2, icon_mappings[0].page_url);
  EXPECT_EQ(2, icon_mappings[0].icon_id);
  ASSERT_TRUE(db.GetFavicon(
      icon_mappings[0].icon_id, &time, &out_data, &out_icon_url));
  EXPECT_EQ(icon2, out_icon_url);

  // Test a page without icon
  GURL page_url4 = GURL("http://www.google.com/blank.html");
  EXPECT_FALSE(db.GetIconMappingsForPageURL(page_url4, NULL));
}

}  // namespace history
