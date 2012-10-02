// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/compiler_specific.h"
#include "chrome/browser/download/test_download_shelf.h"

TEST(DownloadShelfTest, ClosesShelfWhenHidden) {
  TestDownloadShelf shelf;
  shelf.Show();
  EXPECT_TRUE(shelf.IsShowing());
  shelf.Hide();
  EXPECT_FALSE(shelf.IsShowing());
  shelf.Unhide();
  EXPECT_TRUE(shelf.IsShowing());
}

TEST(DownloadShelfTest, CloseWhileHiddenPreventsShowOnUnhide) {
  TestDownloadShelf shelf;
  shelf.Show();
  shelf.Hide();
  shelf.Close();
  shelf.Unhide();
  EXPECT_FALSE(shelf.IsShowing());
}

TEST(DownloadShelfTest, UnhideDoesntShowIfNotShownOnHide) {
  TestDownloadShelf shelf;
  shelf.Hide();
  shelf.Unhide();
  EXPECT_FALSE(shelf.IsShowing());
}

TEST(DownloadShelfTest, AddDownloadWhileHiddenUnhides) {
  TestDownloadShelf shelf;
  shelf.Show();
  shelf.Hide();
  shelf.AddDownload(NULL);
  EXPECT_TRUE(shelf.IsShowing());
}

TEST(DownloadShelfTest, AddDownloadWhileHiddenUnhidesAndShows) {
  TestDownloadShelf shelf;
  shelf.Hide();
  shelf.AddDownload(NULL);
  EXPECT_TRUE(shelf.IsShowing());
}
