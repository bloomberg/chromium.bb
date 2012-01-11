// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_unittest_utils.h"

#include "base/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"

bool EqualBookmarkEntry(const ProfileWriter::BookmarkEntry& entry,
                        const BookmarkInfo& expected) {
  if (expected.in_toolbar != entry.in_toolbar ||
      expected.path_size != entry.path.size() ||
      expected.url != entry.url.spec() ||
      WideToUTF16Hack(expected.title) != entry.title)
    return false;
  for (size_t i = 0; i < expected.path_size; ++i) {
    if (WideToUTF16Hack(expected.path[i]) != entry.path[i])
      return false;
  }
  return true;
}

bool FindBookmarkEntry(const ProfileWriter::BookmarkEntry& entry,
                       const BookmarkInfo* list, int list_size) {
  for (int i = 0; i < list_size; ++i) {
    if (EqualBookmarkEntry(entry, list[i]))
      return true;
  }
  return false;
}

ImporterTest::ImporterTest()
    : profile_(new TestingProfile()),
      ui_thread_(content::BrowserThread::UI, &message_loop_),
      file_thread_(content::BrowserThread::FILE, &message_loop_) {
}

ImporterTest::~ImporterTest() {
  profile_.reset(NULL);
}

void ImporterTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}
