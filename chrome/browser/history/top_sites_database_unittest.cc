// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/top_sites_database.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// URL with url_rank 0 in golden files.
const GURL kUrl = GURL("http://www.google.com/");

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

// Verify that the up-to-date database has the expected tables and
// columns.  Functional tests only check whether the things which
// should be there are, but do not check if extraneous items are
// present.  Any extraneous items have the potential to interact
// negatively with future schema changes.
void VerifyTablesAndColumns(sql::Connection* db) {
  // [meta] and [thumbnails].
  EXPECT_EQ(2u, sql::test::CountSQLTables(db));

  // Implicit index on [meta], index on [thumbnails].
  EXPECT_EQ(2u, sql::test::CountSQLIndices(db));

  // [key] and [value].
  EXPECT_EQ(2u, sql::test::CountTableColumns(db, "meta"));

  // [url], [url_rank], [title], [thumbnail], [redirects],
  // [boring_score], [good_clipping], [at_top], [last_updated], and
  // [load_completed], [last_forced]
  EXPECT_EQ(11u, sql::test::CountTableColumns(db, "thumbnails"));
}

}  // namespace

namespace history {

class TopSitesDatabaseTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_name_ = temp_dir_.path().AppendASCII("TestTopSites.db");
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
};

TEST_F(TopSitesDatabaseTest, Version1) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v1.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  VerifyTablesAndColumns(db.db_.get());

  // Basic operational check.
  MostVisitedURLList urls;
  std::map<GURL, Images> thumbnails;
  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(3u, urls.size());
  ASSERT_EQ(3u, thumbnails.size());
  EXPECT_EQ(kUrl, urls[0].url);  // [0] because of url_rank.
  // kGoogleThumbnail includes nul terminator.
  ASSERT_EQ(sizeof(kGoogleThumbnail) - 1,
            thumbnails[urls[0].url].thumbnail->size());
  EXPECT_TRUE(!memcmp(thumbnails[urls[0].url].thumbnail->front(),
                      kGoogleThumbnail, sizeof(kGoogleThumbnail) - 1));

  ASSERT_TRUE(db.RemoveURL(urls[1]));
  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(2u, thumbnails.size());
}

TEST_F(TopSitesDatabaseTest, Version2) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v2.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  VerifyTablesAndColumns(db.db_.get());

  // Basic operational check.
  MostVisitedURLList urls;
  std::map<GURL, Images> thumbnails;
  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(3u, urls.size());
  ASSERT_EQ(3u, thumbnails.size());
  EXPECT_EQ(kUrl, urls[0].url);  // [0] because of url_rank.
  // kGoogleThumbnail includes nul terminator.
  ASSERT_EQ(sizeof(kGoogleThumbnail) - 1,
            thumbnails[urls[0].url].thumbnail->size());
  EXPECT_TRUE(!memcmp(thumbnails[urls[0].url].thumbnail->front(),
                      kGoogleThumbnail, sizeof(kGoogleThumbnail) - 1));

  ASSERT_TRUE(db.RemoveURL(urls[1]));
  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(2u, thumbnails.size());
}

TEST_F(TopSitesDatabaseTest, Version3) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v3.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  VerifyTablesAndColumns(db.db_.get());

  // Basic operational check.
  MostVisitedURLList urls;
  std::map<GURL, Images> thumbnails;
  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(3u, urls.size());
  ASSERT_EQ(3u, thumbnails.size());
  EXPECT_EQ(kUrl, urls[0].url);  // [0] because of url_rank.
  // kGoogleThumbnail includes nul terminator.
  ASSERT_EQ(sizeof(kGoogleThumbnail) - 1,
            thumbnails[urls[0].url].thumbnail->size());
  EXPECT_TRUE(!memcmp(thumbnails[urls[0].url].thumbnail->front(),
                      kGoogleThumbnail, sizeof(kGoogleThumbnail) - 1));

  ASSERT_TRUE(db.RemoveURL(urls[1]));
  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(2u, thumbnails.size());
}

TEST_F(TopSitesDatabaseTest, AddRemoveEditThumbnails) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, "TopSites.v3.sql"));

  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  // Add a new URL, not forced, rank = 1.
  GURL mapsUrl = GURL("http://maps.google.com/");
  MostVisitedURL url1(mapsUrl, ASCIIToUTF16("Google Maps"));
  db.SetPageThumbnail(url1, 1, Images());

  MostVisitedURLList urls;
  std::map<GURL, Images> thumbnails;
  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(4u, urls.size());
  ASSERT_EQ(4u, thumbnails.size());
  EXPECT_EQ(kUrl, urls[0].url);
  EXPECT_EQ(mapsUrl, urls[1].url);

  // Add a new URL, forced.
  GURL driveUrl = GURL("http://drive.google.com/");
  MostVisitedURL url2(driveUrl, ASCIIToUTF16("Google Drive"));
  url2.last_forced_time = base::Time::FromJsTime(789714000000);  // 10/1/1995
  db.SetPageThumbnail(url2, TopSitesDatabase::kRankOfForcedURL, Images());

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(5u, urls.size());
  ASSERT_EQ(5u, thumbnails.size());
  EXPECT_EQ(driveUrl, urls[0].url);  // Forced URLs always appear first.
  EXPECT_EQ(kUrl, urls[1].url);
  EXPECT_EQ(mapsUrl, urls[2].url);

  // Add a new URL, forced (earlier).
  GURL plusUrl = GURL("http://plus.google.com/");
  MostVisitedURL url3(plusUrl, ASCIIToUTF16("Google Plus"));
  url3.last_forced_time = base::Time::FromJsTime(787035600000);  // 10/12/1994
  db.SetPageThumbnail(url3, TopSitesDatabase::kRankOfForcedURL, Images());

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(6u, urls.size());
  ASSERT_EQ(6u, thumbnails.size());
  EXPECT_EQ(plusUrl, urls[0].url);  // New forced URL should appear first.
  EXPECT_EQ(driveUrl, urls[1].url);
  EXPECT_EQ(kUrl, urls[2].url);
  EXPECT_EQ(mapsUrl, urls[3].url);

  // Change the last_forced_time of a forced URL.
  url3.last_forced_time = base::Time::FromJsTime(792392400000);  // 10/2/1995
  db.SetPageThumbnail(url3, TopSitesDatabase::kRankOfForcedURL, Images());

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(6u, urls.size());
  ASSERT_EQ(6u, thumbnails.size());
  EXPECT_EQ(driveUrl, urls[0].url);
  EXPECT_EQ(plusUrl, urls[1].url);  // Forced URL should have moved second.
  EXPECT_EQ(kUrl, urls[2].url);
  EXPECT_EQ(mapsUrl, urls[3].url);

  // Change a non-forced URL to forced using UpdatePageRank.
  url1.last_forced_time = base::Time::FromJsTime(792219600000);  // 8/2/1995
  db.UpdatePageRank(url1, TopSitesDatabase::kRankOfForcedURL);

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(6u, urls.size());
  ASSERT_EQ(6u, thumbnails.size());
  EXPECT_EQ(driveUrl, urls[0].url);
  EXPECT_EQ(mapsUrl, urls[1].url);  // Maps moves to second forced URL.
  EXPECT_EQ(plusUrl, urls[2].url);
  EXPECT_EQ(kUrl, urls[3].url);

  // Change a forced URL to non-forced using SetPageThumbnail.
  db.SetPageThumbnail(url3, 1, Images());

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(6u, urls.size());
  ASSERT_EQ(6u, thumbnails.size());
  EXPECT_EQ(driveUrl, urls[0].url);
  EXPECT_EQ(mapsUrl, urls[1].url);
  EXPECT_EQ(kUrl, urls[2].url);
  EXPECT_EQ(plusUrl, urls[3].url);  // Plus moves to second non-forced URL.

  // Change a non-forced URL to earlier non-forced using UpdatePageRank.
  url3.last_forced_time = base::Time();
  db.UpdatePageRank(url3, 0);

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(6u, urls.size());
  ASSERT_EQ(6u, thumbnails.size());
  EXPECT_EQ(driveUrl, urls[0].url);
  EXPECT_EQ(mapsUrl, urls[1].url);
  EXPECT_EQ(plusUrl, urls[2].url);  // Plus moves to first non-forced URL.
  EXPECT_EQ(kUrl, urls[3].url);

  // Change a non-forced URL to later non-forced using SetPageThumbnail.
  db.SetPageThumbnail(url3, 2, Images());

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(6u, urls.size());
  ASSERT_EQ(6u, thumbnails.size());
  EXPECT_EQ(driveUrl, urls[0].url);
  EXPECT_EQ(mapsUrl, urls[1].url);
  EXPECT_EQ(kUrl, urls[2].url);
  EXPECT_EQ(plusUrl, urls[4].url);  // Plus moves to third non-forced URL.

  // Remove a non-forced URL.
  db.RemoveURL(url3);

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(5u, urls.size());
  ASSERT_EQ(5u, thumbnails.size());
  EXPECT_EQ(driveUrl, urls[0].url);
  EXPECT_EQ(mapsUrl, urls[1].url);
  EXPECT_EQ(kUrl, urls[2].url);

  // Remove a forced URL.
  db.RemoveURL(url2);

  db.GetPageThumbnails(&urls, &thumbnails);
  ASSERT_EQ(4u, urls.size());
  ASSERT_EQ(4u, thumbnails.size());
  EXPECT_EQ(mapsUrl, urls[0].url);
  EXPECT_EQ(kUrl, urls[1].url);
}

}  // namespace history
