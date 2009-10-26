// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/importer/firefox2_importer.h"

class BookmarkHTMLWriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path_));
    path_ = path_.AppendASCII("bookmarks.html");
    file_util::Delete(path_, true);
  }

  virtual void TearDown() {
    if (!path_.empty())
      file_util::Delete(path_, true);
  }

  // Converts a BookmarkEntry to a string suitable for assertion testing.
  std::wstring BookmarkEntryToString(
      const ProfileWriter::BookmarkEntry& entry) {
    std::wstring result;
    result.append(L"on_toolbar=");
    if (entry.in_toolbar)
      result.append(L"false");
    else
      result.append(L"true");

    result.append(L" url=" + UTF8ToWide(entry.url.spec()));

    result.append(L" path=");
    for (size_t i = 0; i < entry.path.size(); ++i) {
      if (i != 0)
        result.append(L"/");
      result.append(entry.path[i]);
    }

    result.append(L" title=");
    result.append(entry.title);

    result.append(L" time=");
    result.append(base::TimeFormatFriendlyDateAndTime(entry.creation_time));
    return result;
  }

  // Creates a set of bookmark values to a string for assertion testing.
  std::wstring BookmarkValuesToString(bool on_toolbar,
                                      const GURL& url,
                                      const std::wstring& title,
                                      base::Time creation_time,
                                      const std::wstring& f1,
                                      const std::wstring& f2,
                                      const std::wstring& f3) {
    ProfileWriter::BookmarkEntry entry;
    entry.in_toolbar = on_toolbar;
    entry.url = url;
    // The first path element should always be 'x', as that is what we passed
    // to the importer.
    entry.path.push_back(L"x");
    if (!f1.empty()) {
      entry.path.push_back(f1);
      if (!f2.empty()) {
        entry.path.push_back(f2);
        if (!f3.empty())
          entry.path.push_back(f3);
      }
    }
    entry.title = title;
    entry.creation_time = creation_time;
    return BookmarkEntryToString(entry);
  }

  void AssertBookmarkEntryEquals(const ProfileWriter::BookmarkEntry& entry,
                                 bool on_toolbar,
                                 const GURL& url,
                                 const std::wstring& title,
                                 base::Time creation_time,
                                 const std::wstring& f1,
                                 const std::wstring& f2,
                                 const std::wstring& f3) {
    EXPECT_EQ(BookmarkValuesToString(on_toolbar, url, title, creation_time,
                                     f1, f2, f3),
              BookmarkEntryToString(entry));
  }

  FilePath path_;
};

// Tests bookmark_html_writer by populating a BookmarkModel, writing it out by
// way of bookmark_html_writer, then using the importer to read it back in.
// Flaky, http://crbug.com/17592.
TEST_F(BookmarkHTMLWriterTest, FLAKY_Test) {
  // Populate the BookmarkModel. This creates the following bookmark structure:
  // Bookmarks bar
  //   F1
  //     url1
  //     F2
  //       url2
  //   url3
  //   url4
  // Other
  //   url1
  //   url2
  //   F3
  //     F4
  //       url1
  std::wstring f1_title = L"F\"&;<1\"";
  std::wstring f2_title = L"F2";
  std::wstring f3_title = L"F 3";
  std::wstring f4_title = L"F4";
  std::wstring url1_title = L"url 1";
  std::wstring url2_title = L"url&2";
  std::wstring url3_title = L"url\"3";
  std::wstring url4_title = L"url\"&;";
  GURL url1("http://url1");
  GURL url2("http://url2");
  GURL url3("http://url3");
  GURL url4("http://\"&;\"");
  BookmarkModel model(NULL);
  base::Time t1(base::Time::Now());
  base::Time t2(t1 + base::TimeDelta::FromHours(1));
  base::Time t3(t1 + base::TimeDelta::FromHours(1));
  base::Time t4(t1 + base::TimeDelta::FromHours(1));
  const BookmarkNode* f1 = model.AddGroup(
      model.GetBookmarkBarNode(), 0, f1_title);
  model.AddURLWithCreationTime(f1, 0, url1_title, url1, t1);
  const BookmarkNode* f2 = model.AddGroup(f1, 1, f2_title);
  model.AddURLWithCreationTime(f2, 0, url2_title, url2, t2);
  model.AddURLWithCreationTime(model.GetBookmarkBarNode(), 1, url3_title, url3,
                               t3);

  model.AddURLWithCreationTime(model.other_node(), 0, url1_title, url1, t1);
  model.AddURLWithCreationTime(model.other_node(), 1, url2_title, url2, t2);
  const BookmarkNode* f3 = model.AddGroup(model.other_node(), 2, f3_title);
  const BookmarkNode* f4 = model.AddGroup(f3, 0, f4_title);
  model.AddURLWithCreationTime(f4, 0, url1_title, url1, t1);
  model.AddURLWithCreationTime(model.GetBookmarkBarNode(), 2, url4_title,
                              url4, t4);

  // Write to a temp file.
  bookmark_html_writer::WriteBookmarks(NULL, &model, path_);

  // Read the bookmarks back in.
  std::vector<ProfileWriter::BookmarkEntry> parsed_bookmarks;
  Firefox2Importer::ImportBookmarksFile(path_.ToWStringHack(), std::set<GURL>(),
                                        false, L"x", NULL, &parsed_bookmarks,
                                        NULL, NULL);

  // Verify we got back what we wrote.
  ASSERT_EQ(7U, parsed_bookmarks.size());
  // Hardcode the value of IDS_BOOKMARK_BAR_FOLDER_NAME in en-US locale
  // because all the unit tests are run in en-US locale.
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
  // Windows and ChromeOS builds use Sentence case.
  const wchar_t* kBookmarkBarFolderName = L"Bookmarks bar";
#else
  // Mac and Linux + GTK uses Title Case.
  const wchar_t* kBookmarkBarFolderName = L"Bookmarks Bar";
#endif
  AssertBookmarkEntryEquals(parsed_bookmarks[0], false, url1, url1_title, t1,
                            kBookmarkBarFolderName, f1_title, std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[1], false, url2, url2_title, t2,
                            kBookmarkBarFolderName, f1_title, f2_title);
  AssertBookmarkEntryEquals(parsed_bookmarks[2], false, url3, url3_title, t3,
                            kBookmarkBarFolderName, std::wstring(),
                            std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[3], false, url4, url4_title, t4,
                            kBookmarkBarFolderName, std::wstring(),
                            std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[4], false, url1, url1_title, t1,
                            std::wstring(), std::wstring(), std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[5], false, url2, url2_title, t2,
                            std::wstring(), std::wstring(), std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[6], false, url1, url1_title, t1,
                            f3_title, f4_title, std::wstring());
}
