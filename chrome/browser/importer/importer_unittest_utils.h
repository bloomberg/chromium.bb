// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_UNITTEST_UTILS_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_UNITTEST_UTILS_H_

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "chrome/browser/importer/profile_writer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace importer {
class ImporterProgressObserver;
}

class TestingProfile;

const int kMaxPathSize = 5;

struct BookmarkInfo {
  const bool in_toolbar;
  const size_t path_size;
  const wchar_t* path[kMaxPathSize];
  const wchar_t* title;
  const char* url;
};

// Returns true if the |entry| is equal to |expected|.
bool EqualBookmarkEntry(const ProfileWriter::BookmarkEntry& entry,
                        const BookmarkInfo& expected);

// Returns true if the |entry| is in the |list|.
bool FindBookmarkEntry(const ProfileWriter::BookmarkEntry& entry,
                       const BookmarkInfo* list,
                       int list_size);

// Test fixture class providing a message loop, ui/file thread,
// and a scoped temporary directory.
class ImporterTest : public testing::Test {
 public:
  ImporterTest();
  virtual ~ImporterTest();

 protected:
  virtual void SetUp() OVERRIDE;

  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_UNITTEST_UTILS_H_
