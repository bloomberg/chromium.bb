// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/file_path_verifier_win.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(FilePathVerifierTest, ComparePathsIgnoreCase) {
  const struct PathComparisonReasonTestData {
    const base::FilePath::CharType* inputs[2];
    internal::PathComparisonReason expected;
  } cases[] = {
    { { FILE_PATH_LITERAL("test/foo.bar"),
        FILE_PATH_LITERAL("test/foo.bar") },
      internal::PATH_COMPARISON_EQUAL},
    { { FILE_PATH_LITERAL("test\\foo.bar"),
        FILE_PATH_LITERAL("test\\foo.bar") },
      internal::PATH_COMPARISON_EQUAL},
    { { FILE_PATH_LITERAL("test/foo.bar"),
        FILE_PATH_LITERAL("test/foo.baz") },
      internal::PATH_COMPARISON_FAILED_SAMEDIR},
    { { FILE_PATH_LITERAL("test/foo.bar"),
        FILE_PATH_LITERAL("test/joe/foo.bar") },
      internal::PATH_COMPARISON_FAILED_SAMEBASE},
    { { FILE_PATH_LITERAL("test/foo.bar"),
        FILE_PATH_LITERAL("jack/bar.buz") },
      internal::PATH_COMPARISON_FAILED_UNKNOWN},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    base::FilePath p1(cases[i].inputs[0]);
    base::FilePath p2(cases[i].inputs[1]);
    internal::PathComparisonReason reason =
        internal::ComparePathsIgnoreCase(p1, p2);
    EXPECT_EQ(cases[i].expected, reason) <<
        "i: " << i << ", p1: " << p1.value() << ", p2: " << p2.value();
  }
}
