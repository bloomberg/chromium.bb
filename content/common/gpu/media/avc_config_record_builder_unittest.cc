// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/avc_config_record_builder.h"

#include "content/common/gpu/media/h264_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint8 kSPSData[] = {
  0x67, 0x64, 0x00, 0x1f, 0xac, 0x34, 0xec, 0x05,
  0x00, 0x5b, 0xa1, 0x00, 0x00, 0x03, 0x00, 0x01,
  0x00, 0x00, 0x03, 0x00, 0x32, 0x0f, 0x18, 0x31,
  0x38,
};

const uint8 kPPSData[] = {
  0x68, 0xef, 0xb2, 0xc8, 0xb0,
};

const uint8 kNALUHeader[] = {
  0x00, 0x00, 0x00, 0x01
};

}  // namespace

TEST(AVCConfigRecordBuilderTest, BuildConfig) {
  std::vector<uint8> stream;
  stream.insert(stream.end(), kNALUHeader,
                kNALUHeader + arraysize(kNALUHeader));
  stream.insert(stream.end(), kSPSData, kSPSData + arraysize(kSPSData));
  stream.insert(stream.end(), kNALUHeader,
                kNALUHeader + arraysize(kNALUHeader));
  stream.insert(stream.end(), kPPSData, kPPSData + arraysize(kPPSData));

  content::H264Parser parser;
  parser.SetStream(&stream[0], stream.size());
  content::H264NALU nalu;
  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), content::H264Parser::kOk);

  content::AVCConfigRecordBuilder config;
  std::vector<uint8> config_data;
  EXPECT_TRUE(config.ProcessNALU(&parser, nalu, &config_data));
  EXPECT_TRUE(config_data.empty());

  ASSERT_EQ(parser.AdvanceToNextNALU(&nalu), content::H264Parser::kOk);

  EXPECT_TRUE(config.ProcessNALU(&parser, nalu, &config_data));
  EXPECT_TRUE(config_data.empty());

  nalu.nal_unit_type = content::H264NALU::kIDRSlice;
  EXPECT_TRUE(config.ProcessNALU(&parser, nalu, &config_data));
  EXPECT_FALSE(config_data.empty());
  EXPECT_EQ(config.coded_width(), 1280);
  EXPECT_EQ(config.coded_height(), 720);

  const uint8 kExpectedData[] = {
    0x01, 0x64, 0x00, 0x1f,    0xff, 0xe1, 0x00, 0x19,
    0x67, 0x64, 0x00, 0x1f,    0xac, 0x34, 0xec, 0x05,
    0x00, 0x5b, 0xa1, 0x00,    0x00, 0x03, 0x00, 0x01,
    0x00, 0x00, 0x03, 0x00,    0x32, 0x0f, 0x18, 0x31,
    0x38, 0x01, 0x00, 0x05,    0x68, 0xef, 0xb2, 0xc8,
    0xb0,
  };
  std::vector<uint8> expected_config_data(
        kExpectedData, kExpectedData + arraysize(kExpectedData));
  EXPECT_EQ(config_data, expected_config_data);
}

// Test building a config record with zero SPS and PPS data.
TEST(AVCConfigRecordBuilderTest, EmptyConfig) {
  content::AVCConfigRecordBuilder config;
  content::H264Parser parser;
  std::vector<uint8> config_data;

  // Feed the builder a slice right away.
  content::H264NALU nalu;
  nalu.nal_unit_type = content::H264NALU::kIDRSlice;
  EXPECT_TRUE(config.ProcessNALU(&parser, nalu, &config_data));
  EXPECT_FALSE(config_data.empty());
  EXPECT_EQ(config.coded_width(), 0);
  EXPECT_EQ(config.coded_height(), 0);

  const uint8 kExpectedData[] = {
    0x01, 0x0, 0x00, 0x0, 0xff, 0xe0, 0x00,
  };
  std::vector<uint8> expected_config_data(
        kExpectedData, kExpectedData + arraysize(kExpectedData));
  EXPECT_EQ(config_data, expected_config_data);
}
