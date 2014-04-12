// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/utility/media_galleries/image_metadata_extractor.h"
#include "media/filters/file_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metadata {

void QuitLoop(base::RunLoop* loop, bool* output, bool success) {
  loop->Quit();
  *output = success;
}

base::FilePath GetTestDataFilePath(const std::string& filename) {
  base::FilePath path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").AppendASCII("api_test")
      .AppendASCII("wallpaper").AppendASCII(filename);
  return path;
}

scoped_ptr<ImageMetadataExtractor> GetExtractor(
    const std::string& filename,
    bool expected_result) {
  EXPECT_TRUE(ImageMetadataExtractor::InitializeLibraryForTesting());

  media::FileDataSource source;
  base::FilePath test_path;

  EXPECT_TRUE(source.Initialize(GetTestDataFilePath(filename)));

  scoped_ptr<ImageMetadataExtractor> extractor(new ImageMetadataExtractor);

  base::RunLoop loop;
  bool extracted = false;
  extractor->Extract(&source, base::Bind(&QuitLoop, &loop, &extracted));
  EXPECT_EQ(expected_result, extracted);

  return extractor.Pass();
}

TEST(ImageMetadataExtractorTest, JPGFile) {
  scoped_ptr<ImageMetadataExtractor> extractor =
      GetExtractor("test.jpg", true);

  EXPECT_EQ(5616, extractor->width());
  EXPECT_EQ(3744, extractor->height());
  EXPECT_EQ(0, extractor->rotation());
  EXPECT_EQ(300.0, extractor->x_resolution());
  EXPECT_EQ(300.0, extractor->y_resolution());
  EXPECT_EQ("2012:03:01 17:06:07", extractor->date());
  EXPECT_EQ("Canon", extractor->camera_make());
  EXPECT_EQ("Canon EOS 5D Mark II", extractor->camera_model());
  EXPECT_EQ(0.01, extractor->exposure_time_sec());
  EXPECT_FALSE(extractor->flash_fired());
  EXPECT_EQ(3.2, extractor->f_number());
  EXPECT_EQ(100, extractor->focal_length_mm());
  EXPECT_EQ(1600, extractor->iso_equivalent());
}

TEST(ImageMetadataExtractorTest, PNGFile) {
  GetExtractor("test.png", false);
}

TEST(ImageMetadataExtractorTest, NonImageFile) {
  GetExtractor("test.js", false);
}

}  // namespace metadata
