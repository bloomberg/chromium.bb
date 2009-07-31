// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/safari_importer.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "testing/platform_test.h"

// In order to test the Safari import functionality effectively, we store a
// simulated Library directory containing dummy data files in the same
// structure as ~/Library in the Chrome test data directory.
// This function returns the path to that directory.
FilePath GetTestSafariLibraryPath() {
    std::wstring test_dir_wstring;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir_wstring);
    FilePath test_dir = FilePath::FromWStringHack(test_dir_wstring);
    
    // Our simulated ~/Library directory
    test_dir = test_dir.Append("safari_import");
    return test_dir;
}

class SafariImporterTest : public PlatformTest {};

TEST_F(SafariImporterTest, HistoryImport) {
  FilePath test_library_dir = GetTestSafariLibraryPath();
  ASSERT_TRUE(file_util::PathExists(test_library_dir))  <<
      "Missing test data directory";

  scoped_refptr<SafariImporter> importer(
      new SafariImporter(test_library_dir));
  
  std::vector<history::URLRow> history_items;
  importer->ParseHistoryItems(&history_items);
  
  // Should be 2 history items.
  ASSERT_EQ(history_items.size(), 2U);
  
  history::URLRow& it1 = history_items[0];
  EXPECT_EQ(it1.url(), GURL("http://www.firsthistoryitem.com/"));
  EXPECT_EQ(it1.title(), L"First History Item Title");
  EXPECT_EQ(it1.visit_count(), 1);
  EXPECT_EQ(it1.hidden(), 0);
  EXPECT_EQ(it1.typed_count(), 0);
  EXPECT_EQ(it1.last_visit().ToDoubleT(),
      importer->HistoryTimeToEpochTime(@"270598264.4"));
  
  history::URLRow& it2 = history_items[1];
  std::string second_item_title("http://www.secondhistoryitem.com/");
  EXPECT_EQ(it2.url(), GURL(second_item_title));
  // The second item lacks a title so we expect the URL to be substituted.
  EXPECT_EQ(base::SysWideToUTF8(it2.title()), second_item_title.c_str());
  EXPECT_EQ(it2.visit_count(), 55);
  EXPECT_EQ(it2.hidden(), 0);
  EXPECT_EQ(it2.typed_count(), 0);
  EXPECT_EQ(it2.last_visit().ToDoubleT(),
      importer->HistoryTimeToEpochTime(@"270598231.4"));
}
