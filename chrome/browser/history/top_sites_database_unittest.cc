// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/history/top_sites_database.h"
#include "testing/gtest/include/gtest/gtest.h"

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

TEST_F(TopSitesDatabaseTest, UpgradeToVersion2) {
  TopSitesDatabase db;
  ASSERT_TRUE(db.Init(file_name_));

  // Create a version 1 table. SQLite doesn't support DROP COLUMN with
  // ALTER TABLE. Hence, we recreate a table here.
  ASSERT_TRUE(db.db_->Execute("DROP TABLE IF EXISTS thumbnails"));
  ASSERT_TRUE(db.db_->Execute("CREATE TABLE thumbnails ("
                              "url LONGVARCHAR PRIMARY KEY,"
                              "url_rank INTEGER ,"
                              "title LONGVARCHAR,"
                              "thumbnail BLOB,"
                              "redirects LONGVARCHAR,"
                              "boring_score DOUBLE DEFAULT 1.0, "
                              "good_clipping INTEGER DEFAULT 0, "
                              "at_top INTEGER DEFAULT 0, "
                              "last_updated INTEGER DEFAULT 0)"));
  db.meta_table_.SetVersionNumber(1);

  // Make sure the table meets the version 1 criteria.
  ASSERT_EQ(1, db.meta_table_.GetVersionNumber());
  ASSERT_FALSE(db.db_->DoesColumnExist("thumbnails", "load_completed"));

  // Upgrade to version 2.
  ASSERT_TRUE(db.UpgradeToVersion2());

  // Make sure the table meets the version 2 criteria.
  ASSERT_EQ(2, db.meta_table_.GetVersionNumber());
  ASSERT_TRUE(db.db_->DoesColumnExist("thumbnails", "load_completed"));
}

}  // namespace history
