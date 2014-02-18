// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "media/base/audio_video_metadata_extractor.h"
#include "media/base/test_data_util.h"
#include "media/filters/file_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

scoped_ptr<AudioVideoMetadataExtractor> GetExtractor(
    const std::string& filename,
    bool expected_result,
    double expected_duration,
    int expected_width,
    int expected_height) {
  FileDataSource source;
  EXPECT_TRUE(source.Initialize(GetTestDataFilePath(filename)));

  scoped_ptr<AudioVideoMetadataExtractor> extractor(
      new AudioVideoMetadataExtractor);
  bool extracted = extractor->Extract(&source);
  EXPECT_EQ(expected_result, extracted);

  if (!extracted)
    return extractor.Pass();

  EXPECT_EQ(expected_duration, extractor->duration());

  EXPECT_EQ(expected_width, extractor->width());
  EXPECT_EQ(expected_height, extractor->height());

  return extractor.Pass();
}

TEST(AudioVideoMetadataExtractorTest, InvalidFile) {
  GetExtractor("ten_byte_file", false, 0, -1, -1);
}

TEST(AudioVideoMetadataExtractorTest, AudioOGG) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("9ch.ogg", true, 0, -1, -1);
  EXPECT_EQ("Processed by SoX", extractor->comment());
}

TEST(AudioVideoMetadataExtractorTest, AudioWAV) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("sfx_u8.wav", true, 0, -1, -1);
  EXPECT_EQ("Lavf54.37.100", extractor->encoder());
  EXPECT_EQ("Amadeus Pro", extractor->encoded_by());
}

TEST(AudioVideoMetadataExtractorTest, VideoWebM) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("bear-320x240-multitrack.webm", true, 2, 320, 240);
  EXPECT_EQ("Lavf53.9.0", extractor->encoder());
}

#if defined(USE_PROPRIETARY_CODECS)
TEST(AudioVideoMetadataExtractorTest, AndroidRotatedMP4Video) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("90rotation.mp4", true, 0, 1920, 1080);

  EXPECT_EQ(90, extractor->rotation());
}

TEST(AudioVideoMetadataExtractorTest, AudioMP3) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("id3_png_test.mp3", true, 1, -1, -1);

  EXPECT_EQ("Airbag", extractor->title());
  EXPECT_EQ("Radiohead", extractor->artist());
  EXPECT_EQ("OK Computer", extractor->album());
  EXPECT_EQ(1, extractor->track());
  EXPECT_EQ("Alternative", extractor->genre());
  EXPECT_EQ("1997", extractor->date());
  EXPECT_EQ("Lavf54.4.100", extractor->encoder());
}
#endif

}  // namespace media
