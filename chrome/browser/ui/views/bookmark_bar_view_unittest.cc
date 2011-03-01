// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/ui/views/bookmark_bar_view.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"

class BookmarkBarViewTest : public BrowserWithTestWindowTest {
 public:
  BookmarkBarViewTest()
      : file_thread_(BrowserThread::FILE, message_loop()) {}

 private:
  BrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewTest);
};

TEST_F(BookmarkBarViewTest, SwitchProfile) {
  profile()->CreateBookmarkModel(true);
  profile()->BlockUntilBookmarkModelLoaded();

  profile()->GetBookmarkModel()->AddURL(
      profile()->GetBookmarkModel()->GetBookmarkBarNode(),
      0,
      ASCIIToUTF16("blah"),
      GURL("http://www.google.com"));

  BookmarkBarView bookmark_bar(profile(), browser());

  EXPECT_EQ(1, bookmark_bar.GetBookmarkButtonCount());

  TestingProfile profile2;
  profile2.CreateBookmarkModel(true);
  profile2.BlockUntilBookmarkModelLoaded();

  bookmark_bar.SetProfile(&profile2);

  EXPECT_EQ(0, bookmark_bar.GetBookmarkButtonCount());

  bookmark_bar.SetProfile(profile());
  EXPECT_EQ(1, bookmark_bar.GetBookmarkButtonCount());
}
