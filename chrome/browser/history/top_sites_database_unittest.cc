// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
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
  // [load_completed].
  EXPECT_EQ(10u, sql::test::CountTableColumns(db, "thumbnails"));
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

}  // namespace history
