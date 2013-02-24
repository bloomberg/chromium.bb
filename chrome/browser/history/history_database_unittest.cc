// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_database.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "sql/init_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

TEST(HistoryDatabaseTest, DropBookmarks) {
  base::ScopedTempDir temp_dir;
  base::FilePath db_file;

  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  db_file = temp_dir.path().AppendASCII("DropBookmarks.db");
  file_util::Delete(db_file, false);

  // Copy db file over that contains starred URLs.
  base::FilePath old_history_path;
  PathService::Get(chrome::DIR_TEST_DATA, &old_history_path);
  old_history_path = old_history_path.AppendASCII("bookmarks");
  old_history_path = old_history_path.Append(
      FILE_PATH_LITERAL("History_with_starred"));
  file_util::CopyFile(old_history_path, db_file);

  // Load the DB twice. The first time it should migrate. Make sure that the
  // migration leaves it in a state fit to load again later.
  for (int i = 0; i < 2; ++i) {
    HistoryDatabase history_db;
    ASSERT_EQ(sql::INIT_OK, history_db.Init(db_file, NULL));
    HistoryDatabase::URLEnumerator url_enumerator;
    ASSERT_TRUE(history_db.InitURLEnumeratorForEverything(&url_enumerator));
    int num_urls = 0;
    URLRow url_row;
    while (url_enumerator.GetNextURL(&url_row)) {
      ++num_urls;
    }
    ASSERT_EQ(5, num_urls);
  }
}

}  // namespace history
