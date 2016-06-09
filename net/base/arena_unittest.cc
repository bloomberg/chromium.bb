// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/arena.h"

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;

namespace net {
namespace {

size_t kDefaultBlockSize = 2048;
const char kTestString[] = "This is a decently long test string.";

TEST(UnsafeArenaTest, Memdup) {
  UnsafeArena arena(kDefaultBlockSize);
  const size_t length = strlen(kTestString);
  char* c = arena.Memdup(kTestString, length);
  EXPECT_NE(nullptr, c);
  EXPECT_NE(c, kTestString);
  EXPECT_EQ(StringPiece(c, length), kTestString);
}

TEST(UnsafeArenaTest, MemdupLargeString) {
  UnsafeArena arena(10 /* block size */);
  const size_t length = strlen(kTestString);
  char* c = arena.Memdup(kTestString, length);
  EXPECT_NE(nullptr, c);
  EXPECT_NE(c, kTestString);
  EXPECT_EQ(StringPiece(c, length), kTestString);
}

TEST(UnsafeArenaTest, MultipleBlocks) {
  UnsafeArena arena(40 /* block size */);
  std::vector<std::string> strings = {
      "One decently long string.", "Another string.",
      "A third string that will surely go in a different block."};
  std::vector<StringPiece> copies;
  for (const std::string& s : strings) {
    StringPiece sp(arena.Memdup(s.data(), s.size()), s.size());
    copies.push_back(sp);
  }
  EXPECT_EQ(strings.size(), copies.size());
  for (size_t i = 0; i < strings.size(); ++i) {
    EXPECT_EQ(copies[i], strings[i]);
  }
}

TEST(UnsafeArenaTest, UseAfterReset) {
  UnsafeArena arena(kDefaultBlockSize);
  const size_t length = strlen(kTestString);
  char* c = arena.Memdup(kTestString, length);
  arena.Reset();
  c = arena.Memdup(kTestString, length);
  EXPECT_NE(nullptr, c);
  EXPECT_NE(c, kTestString);
  EXPECT_EQ(StringPiece(c, length), kTestString);
}

}  // namespace
}  // namespace net
