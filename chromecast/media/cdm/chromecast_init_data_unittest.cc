// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/chromecast_init_data.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

TEST(ChromecastInitDataTest, TestLegacyUuidCustomData) {
  const uint8_t kUuidBlob[] = {
      0x00, 0x00, 0x00, 0x28,  // length
      0x75, 0x75, 0x69, 0x64,  // 'uuid'
      0x2B, 0xF8, 0x66, 0x80, 0xC6, 0xE5, 0x4E, 0x24, 0xBE,
      0x23, 0x0F, 0x81, 0x5A, 0x60, 0x6E, 0xB2,  // UUID
      0x54, 0x65, 0x73, 0x74, 0x20, 0x63, 0x75, 0x73, 0x74,
      0x6F, 0x6D, 0x20, 0x64, 0x61, 0x74, 0x61  // 'Test custom data'
  };

  ChromecastInitData init_data;
  EXPECT_TRUE(FindChromecastInitData(
      std::vector<uint8_t>(kUuidBlob, kUuidBlob + sizeof(kUuidBlob)),
      InitDataMessageType::CUSTOM_DATA, &init_data));

  EXPECT_EQ(InitDataMessageType::CUSTOM_DATA, init_data.type);
  EXPECT_EQ(16u, init_data.data.size());
  EXPECT_EQ("Test custom data",
            std::string(init_data.data.begin(), init_data.data.end()));
}

TEST(ChromecastInitDataTest, TestUuidAfterPssh) {
  const uint8_t kInitDataBlob[] = {
      0x00, 0x00, 0x00, 0x1C,  // length
      0x70, 0x73, 0x73, 0x68,  // 'pssh'
      0x00, 0x00, 0x00, 0x00,  // version / flags
      0x9a, 0x04, 0xf0, 0x79, 0x98, 0x40, 0x42, 0x86, 0xab,
      0x92, 0xe6, 0x5b, 0xe0, 0x88, 0x5f, 0x95,  // UUID

      0x00, 0x00, 0x00, 0x28,  // length
      0x75, 0x75, 0x69, 0x64,  // 'uuid'
      0x2B, 0xF8, 0x66, 0x80, 0xC6, 0xE5, 0x4E, 0x24, 0xBE,
      0x23, 0x0F, 0x81, 0x5A, 0x60, 0x6E, 0xB2,  // UUID
      0x54, 0x65, 0x73, 0x74, 0x20, 0x63, 0x75, 0x73, 0x74,
      0x6F, 0x6D, 0x20, 0x64, 0x61, 0x74, 0x61  // 'Test custom data'
  };

  ChromecastInitData init_data;
  EXPECT_TRUE(FindChromecastInitData(
      std::vector<uint8_t>(kInitDataBlob,
                           kInitDataBlob + sizeof(kInitDataBlob)),
      InitDataMessageType::CUSTOM_DATA, &init_data));

  EXPECT_EQ(InitDataMessageType::CUSTOM_DATA, init_data.type);
  EXPECT_EQ(16u, init_data.data.size());
  EXPECT_EQ("Test custom data",
            std::string(init_data.data.begin(), init_data.data.end()));
}

TEST(ChromecastInitDataTest, TestPsshCustomData) {
  const uint8_t kInitDataBlob[] = {
      0x00, 0x00, 0x00, 0x32,  // length
      0x70, 0x73, 0x73, 0x68,  // 'pssh'
      0x00, 0x00, 0x00, 0x00,  // version / flags
      0x2B, 0xF8, 0x66, 0x80, 0xC6, 0xE5, 0x4E, 0x24, 0xBE,
      0x23, 0x0F, 0x81, 0x5A, 0x60, 0x6E, 0xB2,  // UUID
      0x00, 0x00, 0x00, 0x12,                    // data size
      0x00, 0x01,                                // message type (CUSTOM_DATA)
      0x54, 0x65, 0x73, 0x74, 0x20, 0x63, 0x75, 0x73, 0x74,
      0x6F, 0x6D, 0x20, 0x64, 0x61, 0x74, 0x61  // 'Test custom data'
  };

  ChromecastInitData init_data;
  EXPECT_TRUE(FindChromecastInitData(
      std::vector<uint8_t>(kInitDataBlob,
                           kInitDataBlob + sizeof(kInitDataBlob)),
      InitDataMessageType::CUSTOM_DATA, &init_data));

  EXPECT_EQ(InitDataMessageType::CUSTOM_DATA, init_data.type);
  EXPECT_EQ(16u, init_data.data.size());
  EXPECT_EQ("Test custom data",
            std::string(init_data.data.begin(), init_data.data.end()));
}

TEST(ChromecastInitDataTest, TestPsshCustomData_NoSize) {
  const uint8_t kInitDataBlob[] = {
      0x00, 0x00, 0x00, 0x2E,  // length
      0x70, 0x73, 0x73, 0x68,  // 'pssh'
      0x00, 0x00, 0x00, 0x00,  // version / flags
      0x2B, 0xF8, 0x66, 0x80, 0xC6, 0xE5, 0x4E, 0x24, 0xBE,
      0x23, 0x0F, 0x81, 0x5A, 0x60, 0x6E, 0xB2,  // UUID
      // [missing size should be present here].
      0x00, 0x01,                                // message type (CUSTOM_DATA)
      0x54, 0x65, 0x73, 0x74, 0x20, 0x63, 0x75, 0x73, 0x74,
      0x6F, 0x6D, 0x20, 0x64, 0x61, 0x74, 0x61  // 'Test custom data'
  };

  ChromecastInitData init_data;
  EXPECT_FALSE(FindChromecastInitData(
      std::vector<uint8_t>(kInitDataBlob,
                           kInitDataBlob + sizeof(kInitDataBlob)),
      InitDataMessageType::CUSTOM_DATA, &init_data));
}

TEST(ChromecastInitDataTest, TestPsshSecureStop) {
  const uint8_t kInitDataBlob[] = {
      0x00, 0x00, 0x00, 0x22,  // length
      0x70, 0x73, 0x73, 0x68,  // 'pssh'
      0x00, 0x00, 0x00, 0x00,  // version / flags
      0x2B, 0xF8, 0x66, 0x80, 0xC6, 0xE5, 0x4E, 0x24,
      0xBE, 0x23, 0x0F, 0x81, 0x5A, 0x60, 0x6E, 0xB2,  // UUID
      0x00, 0x00, 0x00, 0x02, // data size
      0x00, 0x02,  // message type (ENABLE_SECURE_STOP)
  };

  ChromecastInitData init_data;
  EXPECT_TRUE(FindChromecastInitData(
      std::vector<uint8_t>(kInitDataBlob,
                           kInitDataBlob + sizeof(kInitDataBlob)),
      InitDataMessageType::ENABLE_SECURE_STOP, &init_data));

  EXPECT_EQ(InitDataMessageType::ENABLE_SECURE_STOP, init_data.type);
  EXPECT_EQ(0u, init_data.data.size());
}

TEST(ChromecastInitDataTest, TestPsshSecureStop_NoSize) {
  const uint8_t kInitDataBlob[] = {
      0x00, 0x00, 0x00, 0x1E,  // length
      0x70, 0x73, 0x73, 0x68,  // 'pssh'
      0x00, 0x00, 0x00, 0x00,  // version / flags
      0x2B, 0xF8, 0x66, 0x80, 0xC6, 0xE5, 0x4E, 0x24,
      0xBE, 0x23, 0x0F, 0x81, 0x5A, 0x60, 0x6E, 0xB2,  // UUID
      // [missing size should be present here].
      0x00, 0x02,  // message type (ENABLE_SECURE_STOP)
  };

  ChromecastInitData init_data;
  EXPECT_FALSE(FindChromecastInitData(
      std::vector<uint8_t>(kInitDataBlob,
                           kInitDataBlob + sizeof(kInitDataBlob)),
      InitDataMessageType::ENABLE_SECURE_STOP, &init_data));
}

}  // namespace media
}  // namespace chromecast
