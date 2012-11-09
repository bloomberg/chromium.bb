// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/test/chromedriver/chrome_finder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool PathIn(const std::vector<FilePath>& list,
            const FilePath& path) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i] == path)
      return true;
  }
  return false;
}

void AssertFound(const FilePath& found,
                 const std::vector<FilePath>& existing_paths,
                 const std::vector<FilePath>& rel_paths,
                 const std::vector<FilePath>& locations) {
  FilePath exe;
  ASSERT_TRUE(internal::FindExe(
      base::Bind(&PathIn, existing_paths),
      rel_paths,
      locations,
      &exe));
  ASSERT_EQ(found, exe);
}

}  // namespace

TEST(ChromeFinderTest, FindExeFound) {
  FilePath found = FilePath().AppendASCII("exists").AppendASCII("exists");
  std::vector<FilePath> existing_paths;
  existing_paths.push_back(found);
  std::vector<FilePath> rel_paths;
  rel_paths.push_back(found.BaseName());
  std::vector<FilePath> locations;
  locations.push_back(found.DirName());
  ASSERT_NO_FATAL_FAILURE(
      AssertFound(found, existing_paths, rel_paths, locations));
}

TEST(ChromeFinderTest, FindExeShouldGoInOrder) {
  FilePath dir(FILE_PATH_LITERAL("dir"));
  FilePath first = dir.AppendASCII("first");
  FilePath second = dir.AppendASCII("second");
  std::vector<FilePath> existing_paths;
  existing_paths.push_back(first);
  existing_paths.push_back(second);
  std::vector<FilePath> rel_paths;
  rel_paths.push_back(first.BaseName());
  rel_paths.push_back(second.BaseName());
  std::vector<FilePath> locations;
  locations.push_back(dir);
  ASSERT_NO_FATAL_FAILURE(
      AssertFound(first, existing_paths, rel_paths, locations));
}

TEST(ChromeFinderTest, FindExeShouldPreferExeNameOverDir) {
  FilePath dir1(FILE_PATH_LITERAL("dir1"));
  FilePath dir2(FILE_PATH_LITERAL("dir2"));
  FilePath preferred(FILE_PATH_LITERAL("preferred"));
  FilePath nonpreferred(FILE_PATH_LITERAL("nonpreferred"));
  std::vector<FilePath> existing_paths;
  existing_paths.push_back(dir2.Append(preferred));
  existing_paths.push_back(dir1.Append(nonpreferred));
  std::vector<FilePath> rel_paths;
  rel_paths.push_back(preferred);
  rel_paths.push_back(nonpreferred);
  std::vector<FilePath> locations;
  locations.push_back(dir1);
  locations.push_back(dir2);
  ASSERT_NO_FATAL_FAILURE(AssertFound(
      dir2.Append(preferred), existing_paths, rel_paths, locations));
}

TEST(ChromeFinderTest, FindExeNotFound) {
  FilePath found = FilePath().AppendASCII("exists").AppendASCII("exists");
  std::vector<FilePath> existing_paths;
  std::vector<FilePath> rel_paths;
  rel_paths.push_back(found.BaseName());
  std::vector<FilePath> locations;
  locations.push_back(found.DirName());
  FilePath exe;
  ASSERT_FALSE(internal::FindExe(
      base::Bind(&PathIn, existing_paths),
      rel_paths,
      locations,
      &exe));
}

TEST(ChromeFinderTest, NoCrash) {
  // It's not worthwhile to check the validity of the path, so just check
  // for crashes.
  FilePath exe;
  FindChrome(&exe);
}
