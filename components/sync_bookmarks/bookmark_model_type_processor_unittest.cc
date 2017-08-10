// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace sync_bookmarks {

namespace {

class BookmarkModelTypeProcessorTest : public testing::Test {};

TEST_F(BookmarkModelTypeProcessorTest, CreateInstance) {
  BookmarkModelTypeProcessor processor;
}

}  // namespace

}  // namespace sync_bookmarks
