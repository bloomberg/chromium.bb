// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"

class BookmarkBarViewTest : public BrowserWithTestWindowTest {
 public:
  BookmarkBarViewTest()
      : ui_thread_(ChromeThread::UI, message_loop()),
        file_thread_(ChromeThread::FILE, message_loop()) {}

 private:
  ChromeThread ui_thread_;
  ChromeThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewTest);
};

TEST_F(BookmarkBarViewTest, SwitchProfile) {
  profile()->CreateBookmarkModel(true);
  profile()->BlockUntilBookmarkModelLoaded();

  profile()->GetBookmarkModel()->AddURL(
      profile()->GetBookmarkModel()->GetBookmarkBarNode(),
      0,
      L"blah",
      GURL("http://www.google.com"));

  BookmarkBarView bookmark_bar(profile(), browser());

  EXPECT_EQ(1, bookmark_bar.GetBookmarkButtonCount());

  TestingProfile profile2(0);
  profile2.CreateBookmarkModel(true);
  profile2.BlockUntilBookmarkModelLoaded();

  bookmark_bar.SetProfile(&profile2);

  EXPECT_EQ(0, bookmark_bar.GetBookmarkButtonCount());

  bookmark_bar.SetProfile(profile());
  EXPECT_EQ(1, bookmark_bar.GetBookmarkButtonCount());
}
