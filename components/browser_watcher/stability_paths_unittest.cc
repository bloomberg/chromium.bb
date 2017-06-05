// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/stability_paths.h"

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

TEST(StabilityPathsTest, GetStabilityFiles) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create files.
  std::vector<base::FilePath> expected_paths;
  std::set<base::FilePath> excluded_paths;
  {
    // Matches the pattern.
    base::FilePath path = temp_dir.GetPath().AppendASCII("foo1.pma");
    base::ScopedFILE file(base::OpenFile(path, "w"));
    ASSERT_NE(file.get(), nullptr);
    expected_paths.push_back(path);

    // Matches the pattern, but is excluded.
    path = temp_dir.GetPath().AppendASCII("foo2.pma");
    file.reset(base::OpenFile(path, "w"));
    ASSERT_NE(file.get(), nullptr);
    ASSERT_TRUE(excluded_paths.insert(path).second);

    // Matches the pattern.
    path = temp_dir.GetPath().AppendASCII("foo3.pma");
    file.reset(base::OpenFile(path, "w"));
    ASSERT_NE(file.get(), nullptr);
    expected_paths.push_back(path);

    // Does not match the pattern.
    path = temp_dir.GetPath().AppendASCII("bar.baz");
    file.reset(base::OpenFile(path, "w"));
    ASSERT_NE(file.get(), nullptr);
  }

  EXPECT_THAT(GetStabilityFiles(temp_dir.GetPath(),
                                FILE_PATH_LITERAL("foo*.pma"), excluded_paths),
              testing::UnorderedElementsAreArray(expected_paths));
}

}  // namespace browser_watcher
