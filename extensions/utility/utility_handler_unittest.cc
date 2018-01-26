// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "extensions/utility/utility_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using UtilityHandlerTest = testing::Test;

struct UnzipFileFilterTestCase {
  const base::FilePath::CharType* input;
  const bool should_unzip;
};

void RunZipFileFilterTest(
    const std::vector<UnzipFileFilterTestCase>& cases,
    base::RepeatingCallback<bool(const base::FilePath&)>& filter) {
  for (size_t i = 0; i < cases.size(); ++i) {
    base::FilePath input(cases[i].input);
    bool observed = filter.Run(input);
    EXPECT_EQ(cases[i].should_unzip, observed)
        << "i: " << i << ", input: " << input.value();
  }
}

TEST_F(UtilityHandlerTest, NonTheme_FileExtractionFilter) {
  const std::vector<UnzipFileFilterTestCase> cases = {
      {FILE_PATH_LITERAL("foo"), true},
      {FILE_PATH_LITERAL("foo.nexe"), true},
      {FILE_PATH_LITERAL("foo.dll"), true},
      {FILE_PATH_LITERAL("foo.jpg.exe"), false},
      {FILE_PATH_LITERAL("foo.exe"), false},
      {FILE_PATH_LITERAL("foo.EXE"), false},
      {FILE_PATH_LITERAL("file_without_extension"), true},
  };
  base::RepeatingCallback<bool(const base::FilePath&)> filter =
      base::BindRepeating(&utility_handler::ShouldExtractFile, false);
  RunZipFileFilterTest(cases, filter);
}

TEST_F(UtilityHandlerTest, Theme_FileExtractionFilter) {
  const std::vector<UnzipFileFilterTestCase> cases = {
      {FILE_PATH_LITERAL("image.jpg"), true},
      {FILE_PATH_LITERAL("IMAGE.JPEG"), true},
      {FILE_PATH_LITERAL("test/image.bmp"), true},
      {FILE_PATH_LITERAL("test/IMAGE.gif"), true},
      {FILE_PATH_LITERAL("test/image.WEBP"), true},
      {FILE_PATH_LITERAL("test/dir/file.image.png"), true},
      {FILE_PATH_LITERAL("manifest.json"), true},
      {FILE_PATH_LITERAL("other.html"), false},
      {FILE_PATH_LITERAL("file_without_extension"), true},
  };
  base::RepeatingCallback<bool(const base::FilePath&)> filter =
      base::BindRepeating(&utility_handler::ShouldExtractFile, true);
  RunZipFileFilterTest(cases, filter);
}

TEST_F(UtilityHandlerTest, ManifestExtractionFilter) {
  const std::vector<UnzipFileFilterTestCase> cases = {
      {FILE_PATH_LITERAL("manifest.json"), true},
      {FILE_PATH_LITERAL("MANIFEST.JSON"), true},
      {FILE_PATH_LITERAL("test/manifest.json"), false},
      {FILE_PATH_LITERAL("manifest.json/test"), false},
      {FILE_PATH_LITERAL("other.file"), false},
  };
  base::RepeatingCallback<bool(const base::FilePath&)> filter =
      base::BindRepeating(&utility_handler::IsManifestFile);
  RunZipFileFilterTest(cases, filter);
}

}  // namespace extensions
