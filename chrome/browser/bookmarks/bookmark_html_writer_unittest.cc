// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

static const int kIconWidth = 16;
static const int kIconHeight = 16;

void MakeTestSkBitmap(int w, int h, SkBitmap* bmp) {
  bmp->setConfig(SkBitmap::kARGB_8888_Config, w, h);
  bmp->allocPixels();

  uint32_t* src_data = bmp->getAddr32(0, 0);
  for (int i = 0; i < w * h; i++) {
    src_data[i] = SkPreMultiplyARGB(i % 255, i % 250, i % 245, i % 240);
  }
}

}  // namespace

class BookmarkHTMLWriterTest : public TestingBrowserProcessTest {
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
  string16 BookmarkEntryToString(
      const ProfileWriter::BookmarkEntry& entry) {
    string16 result;
    result.append(ASCIIToUTF16("on_toolbar="));
    if (entry.in_toolbar)
      result.append(ASCIIToUTF16("false"));
    else
      result.append(ASCIIToUTF16("true"));

    result.append(ASCIIToUTF16(" url=") + UTF8ToUTF16(entry.url.spec()));

    result.append(ASCIIToUTF16(" path="));
    for (size_t i = 0; i < entry.path.size(); ++i) {
      if (i != 0)
        result.append(ASCIIToUTF16("/"));
      result.append(WideToUTF16Hack(entry.path[i]));
    }

    result.append(ASCIIToUTF16(" title="));
    result.append(WideToUTF16Hack(entry.title));

    result.append(ASCIIToUTF16(" time="));
    result.append(base::TimeFormatFriendlyDateAndTime(entry.creation_time));
    return result;
  }

  // Creates a set of bookmark values to a string for assertion testing.
  string16 BookmarkValuesToString(bool on_toolbar,
                                  const GURL& url,
                                  const string16& title,
                                  base::Time creation_time,
                                  const string16& f1,
                                  const string16& f2,
                                  const string16& f3) {
    ProfileWriter::BookmarkEntry entry;
    entry.in_toolbar = on_toolbar;
    entry.url = url;
    // The first path element should always be 'x', as that is what we passed
    // to the importer.
    entry.path.push_back(L"x");
    if (!f1.empty()) {
      entry.path.push_back(UTF16ToWideHack(f1));
      if (!f2.empty()) {
        entry.path.push_back(UTF16ToWideHack(f2));
        if (!f3.empty())
          entry.path.push_back(UTF16ToWideHack(f3));
      }
    }
    entry.title = UTF16ToWideHack(title);
    entry.creation_time = creation_time;
    return BookmarkEntryToString(entry);
  }

  void AssertBookmarkEntryEquals(const ProfileWriter::BookmarkEntry& entry,
                                 bool on_toolbar,
                                 const GURL& url,
                                 const string16& title,
                                 base::Time creation_time,
                                 const string16& f1,
                                 const string16& f2,
                                 const string16& f3) {
    EXPECT_EQ(BookmarkValuesToString(on_toolbar, url, title, creation_time,
                                     f1, f2, f3),
              BookmarkEntryToString(entry));
  }

  FilePath path_;
};

// Class that will notify message loop when file is written.
class BookmarksObserver : public BookmarksExportObserver {
 public:
  explicit BookmarksObserver(MessageLoop* loop) : loop_(loop) {
    DCHECK(loop);
  }

  virtual void OnExportFinished() {
    loop_->Quit();
  }

 private:
  MessageLoop* loop_;
  DISALLOW_COPY_AND_ASSIGN(BookmarksObserver);
};

// Tests bookmark_html_writer by populating a BookmarkModel, writing it out by
// way of bookmark_html_writer, then using the importer to read it back in.
TEST_F(BookmarkHTMLWriterTest, Test) {
  MessageLoop message_loop;
  BrowserThread fake_ui_thread(BrowserThread::UI, &message_loop);
  BrowserThread fake_file_thread(BrowserThread::FILE, &message_loop);

  TestingProfile profile;
  profile.CreateHistoryService(true, false);
  profile.BlockUntilHistoryProcessesPendingRequests();
  profile.CreateFaviconService();
  profile.CreateBookmarkModel(true);
  profile.BlockUntilBookmarkModelLoaded();
  BookmarkModel* model = profile.GetBookmarkModel();

  // Create test PNG representing favicon for url1.
  SkBitmap bitmap;
  MakeTestSkBitmap(kIconWidth, kIconHeight, &bitmap);
  std::vector<unsigned char> icon_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &icon_data);

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
  string16 f1_title = ASCIIToUTF16("F\"&;<1\"");
  string16 f2_title = ASCIIToUTF16("F2");
  string16 f3_title = ASCIIToUTF16("F 3");
  string16 f4_title = ASCIIToUTF16("F4");
  string16 url1_title = ASCIIToUTF16("url 1");
  string16 url2_title = ASCIIToUTF16("url&2");
  string16 url3_title = ASCIIToUTF16("url\"3");
  string16 url4_title = ASCIIToUTF16("url\"&;");
  GURL url1("http://url1");
  GURL url1_favicon("http://url1/icon.ico");
  GURL url2("http://url2");
  GURL url3("http://url3");
  GURL url4("javascript:alert(\"Hello!\");");
  base::Time t1(base::Time::Now());
  base::Time t2(t1 + base::TimeDelta::FromHours(1));
  base::Time t3(t1 + base::TimeDelta::FromHours(1));
  base::Time t4(t1 + base::TimeDelta::FromHours(1));
  const BookmarkNode* f1 = model->AddGroup(
      model->GetBookmarkBarNode(), 0, f1_title);
  model->AddURLWithCreationTime(f1, 0, url1_title, url1, t1);
  profile.GetHistoryService(Profile::EXPLICIT_ACCESS)->AddPage(url1,
      history::SOURCE_BROWSED);
  profile.GetFaviconService(Profile::EXPLICIT_ACCESS)->SetFavicon(url1,
                                                                  url1_favicon,
                                                                  icon_data);
  message_loop.RunAllPending();
  const BookmarkNode* f2 = model->AddGroup(f1, 1, f2_title);
  model->AddURLWithCreationTime(f2, 0, url2_title, url2, t2);
  model->AddURLWithCreationTime(model->GetBookmarkBarNode(),
                                1, url3_title, url3, t3);

  model->AddURLWithCreationTime(model->other_node(), 0, url1_title, url1, t1);
  model->AddURLWithCreationTime(model->other_node(), 1, url2_title, url2, t2);
  const BookmarkNode* f3 = model->AddGroup(model->other_node(), 2, f3_title);
  const BookmarkNode* f4 = model->AddGroup(f3, 0, f4_title);
  model->AddURLWithCreationTime(f4, 0, url1_title, url1, t1);
  model->AddURLWithCreationTime(model->GetBookmarkBarNode(), 2, url4_title,
                                url4, t4);

  // Write to a temp file.
  BookmarksObserver observer(&message_loop);
  bookmark_html_writer::WriteBookmarks(&profile, path_, &observer);
  message_loop.Run();

  // Clear favicon so that it would be read from file.
  std::vector<unsigned char> empty_data;
  profile.GetFaviconService(Profile::EXPLICIT_ACCESS)->SetFavicon(url1,
                                                                  url1_favicon,
                                                                  empty_data);
  message_loop.RunAllPending();

  // Read the bookmarks back in.
  std::vector<ProfileWriter::BookmarkEntry> parsed_bookmarks;
  std::vector<history::ImportedFavIconUsage> favicons;
  Firefox2Importer::ImportBookmarksFile(path_, std::set<GURL>(),
                                        false, L"x", NULL, &parsed_bookmarks,
                                        NULL, &favicons);

  // Check loaded favicon (url1 is represents by 3 separate bookmarks).
  EXPECT_EQ(3U, favicons.size());
  for (size_t i = 0; i < favicons.size(); i++) {
    if (url1_favicon == favicons[i].favicon_url) {
      EXPECT_EQ(1U, favicons[i].urls.size());
      std::set<GURL>::const_iterator iter = favicons[i].urls.find(url1);
      ASSERT_TRUE(iter != favicons[i].urls.end());
      ASSERT_TRUE(*iter == url1);
      ASSERT_TRUE(favicons[i].png_data == icon_data);
    }
  }

  // Verify we got back what we wrote.
  ASSERT_EQ(7U, parsed_bookmarks.size());
  // Windows and ChromeOS builds use Sentence case.
  string16 bookmark_folder_name =
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_FOLDER_NAME);
  AssertBookmarkEntryEquals(parsed_bookmarks[0], false, url1, url1_title, t1,
                            bookmark_folder_name, f1_title, string16());
  AssertBookmarkEntryEquals(parsed_bookmarks[1], false, url2, url2_title, t2,
                            bookmark_folder_name, f1_title, f2_title);
  AssertBookmarkEntryEquals(parsed_bookmarks[2], false, url3, url3_title, t3,
                            bookmark_folder_name, string16(), string16());
  AssertBookmarkEntryEquals(parsed_bookmarks[3], false, url4, url4_title, t4,
                            bookmark_folder_name, string16(), string16());
  AssertBookmarkEntryEquals(parsed_bookmarks[4], false, url1, url1_title, t1,
                            string16(), string16(), string16());
  AssertBookmarkEntryEquals(parsed_bookmarks[5], false, url2, url2_title, t2,
                            string16(), string16(), string16());
  AssertBookmarkEntryEquals(parsed_bookmarks[6], false, url1, url1_title, t1,
                            f3_title, f4_title, string16());
}
