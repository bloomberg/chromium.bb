// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/compiler_specific.h"
#include "chrome/browser/download/download_shelf.h"

class TestDownloadShelf : public DownloadShelf {
 public:
  TestDownloadShelf() : is_showing_(false) {}

  virtual bool IsShowing() const OVERRIDE {
    return is_showing_;
  }

  virtual bool IsClosing() const OVERRIDE {
    return false;
  }

  virtual Browser* browser() const OVERRIDE {
    return NULL;
  }

 protected:
  virtual void DoAddDownload(BaseDownloadItemModel* download_mode) OVERRIDE {}

  virtual void DoShow() OVERRIDE {
    is_showing_ = true;
  }

  virtual void DoClose() OVERRIDE {
    is_showing_ = false;
  }

 private:
  bool is_showing_;
};

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
