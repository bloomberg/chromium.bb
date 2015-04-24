// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cenc_utils.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const uint8_t kKey1Data[] = {
    0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
    0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03
};
const uint8_t kKey2Data[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
};
const uint8_t kKey3Data[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x05,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x05,
};
const uint8_t kKey4Data[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x06,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x06,
};

class CencUtilsTest : public testing::Test {
 public:
  CencUtilsTest()
      : key1_(kKey1Data, kKey1Data + arraysize(kKey1Data)),
        key2_(kKey2Data, kKey2Data + arraysize(kKey2Data)),
        key3_(kKey3Data, kKey3Data + arraysize(kKey3Data)),
        key4_(kKey4Data, kKey4Data + arraysize(kKey4Data)) {}

 protected:
  // Initialize the start of the 'pssh' box (up to key_count)
  void InitializePSSHBox(std::vector<uint8_t>* box,
                         uint8_t size,
                         uint8_t version) {
    DCHECK(box->size() == 0);

    box->reserve(size);
    // Add size.
    box->push_back(0);
    box->push_back(0);
    box->push_back(0);
    box->push_back(size);
    // Add 'pssh'.
    box->push_back('p');
    box->push_back('s');
    box->push_back('s');
    box->push_back('h');
    // Add version.
    box->push_back(version);
    // Add flags.
    box->push_back(0);
    box->push_back(0);
    box->push_back(0);
    // Add Clear Key SystemID.
    box->push_back(0x10);
    box->push_back(0x77);
    box->push_back(0xEF);
    box->push_back(0xEC);
    box->push_back(0xC0);
    box->push_back(0xB2);
    box->push_back(0x4D);
    box->push_back(0x02);
    box->push_back(0xAC);
    box->push_back(0xE3);
    box->push_back(0x3C);
    box->push_back(0x1E);
    box->push_back(0x52);
    box->push_back(0xE2);
    box->push_back(0xFB);
    box->push_back(0x4B);
  }

  std::vector<uint8_t> MakePSSHBox(uint8_t version) {
    std::vector<uint8_t> box;
    uint8_t size = (version == 0) ? 32 : 36;
    InitializePSSHBox(&box, size, version);
    if (version > 0) {
      // Add key_count (= 0).
      box.push_back(0);
      box.push_back(0);
      box.push_back(0);
      box.push_back(0);
    }
    // Add data_size (= 0).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    return box;
  }

  std::vector<uint8_t> MakePSSHBox(uint8_t version,
                                   const std::vector<uint8_t>& key1) {
    DCHECK(version > 0);
    DCHECK(key1.size() == 16);

    std::vector<uint8_t> box;
    uint8_t size = 52;
    InitializePSSHBox(&box, size, version);

    // Add key_count (= 1).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(1);

    // Add key1.
    for (int i = 0; i < 16; ++i)
      box.push_back(key1[i]);

    // Add data_size (= 0).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    return box;
  }

  std::vector<uint8_t> MakePSSHBox(uint8_t version,
                                   const std::vector<uint8_t>& key1,
                                   const std::vector<uint8_t>& key2) {
    DCHECK(version > 0);
    DCHECK(key1.size() == 16);
    DCHECK(key2.size() == 16);

    std::vector<uint8_t> box;
    uint8_t size = 68;
    InitializePSSHBox(&box, size, version);

    // Add key_count (= 2).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(2);

    // Add key1.
    for (int i = 0; i < 16; ++i)
      box.push_back(key1[i]);

    // Add key2.
    for (int i = 0; i < 16; ++i)
      box.push_back(key2[i]);

    // Add data_size (= 0).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    return box;
  }

  const std::vector<uint8_t>& Key1() { return key1_; }
  const std::vector<uint8_t>& Key2() { return key2_; }
  const std::vector<uint8_t>& Key3() { return key3_; }
  const std::vector<uint8_t>& Key4() { return key4_; }

 private:
  std::vector<uint8_t> key1_;
  std::vector<uint8_t> key2_;
  std::vector<uint8_t> key3_;
  std::vector<uint8_t> key4_;
};

TEST_F(CencUtilsTest, EmptyPSSH) {
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(std::vector<uint8_t>()));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(std::vector<uint8_t>(), &key_ids));
  EXPECT_EQ(0u, key_ids.size());
}

TEST_F(CencUtilsTest, PSSHVersion0) {
  std::vector<uint8_t> box = MakePSSHBox(0);
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(0u, key_ids.size());
}

TEST_F(CencUtilsTest, PSSHVersion1WithNoKeys) {
  std::vector<uint8_t> box = MakePSSHBox(1);
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(0u, key_ids.size());
}

TEST_F(CencUtilsTest, PSSHVersion1WithOneKey) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1());
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(1u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
}

TEST_F(CencUtilsTest, PSSHVersion1WithTwoKeys) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());
  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(2u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
  EXPECT_EQ(key_ids[1], Key2());
}

TEST_F(CencUtilsTest, PSSHVersion0Plus1) {
  std::vector<uint8_t> box0 = MakePSSHBox(0);
  std::vector<uint8_t> box1 = MakePSSHBox(1, Key1());

  // Concatentate box1 into box0.
  for (const auto& value : box1)
    box0.push_back(value);

  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box0));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box0, &key_ids));
  EXPECT_EQ(1u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
}

TEST_F(CencUtilsTest, PSSHVersion1Plus0) {
  std::vector<uint8_t> box0 = MakePSSHBox(0);
  std::vector<uint8_t> box1 = MakePSSHBox(1, Key1());

  // Concatentate box0 into box1.
  for (const auto& value : box0)
    box1.push_back(value);

  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box1));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box1, &key_ids));
  EXPECT_EQ(1u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
}

TEST_F(CencUtilsTest, MultiplePSSHVersion1) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());
  std::vector<uint8_t> box1 = MakePSSHBox(1, Key3());
  std::vector<uint8_t> box2 = MakePSSHBox(1, Key4());

  // Concatentate box1 into box.
  for (const auto& value : box1)
    box.push_back(value);
  // Concatentate box2 into box.
  for (const auto& value : box2)
    box.push_back(value);

  KeyIdList key_ids;
  EXPECT_TRUE(ValidatePsshInput(box));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(4u, key_ids.size());
  EXPECT_EQ(key_ids[0], Key1());
  EXPECT_EQ(key_ids[1], Key2());
  EXPECT_EQ(key_ids[2], Key3());
  EXPECT_EQ(key_ids[3], Key4());
}

TEST_F(CencUtilsTest, InvalidPSSH) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());
  KeyIdList key_ids;
  for (uint32 i = 1; i < box.size(); ++i) {
    // Modify size of data passed to be less than real size.
    std::vector<uint8_t> truncated(&box[0], &box[0] + i);
    EXPECT_FALSE(ValidatePsshInput(truncated));
    EXPECT_FALSE(GetKeyIdsForCommonSystemId(truncated, &key_ids));
    // Modify starting point.
    std::vector<uint8_t> changed_offset(&box[i], &box[i] + box.size() - i);
    EXPECT_FALSE(ValidatePsshInput(changed_offset));
    EXPECT_FALSE(GetKeyIdsForCommonSystemId(changed_offset, &key_ids));
  }
}

TEST_F(CencUtilsTest, InvalidSystemID) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());

  // Modify the System ID.
  ++box[20];

  KeyIdList key_ids;
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(0u, key_ids.size());
}

TEST_F(CencUtilsTest, InvalidFlags) {
  std::vector<uint8_t> box = MakePSSHBox(1, Key1(), Key2());

  // Modify flags.
  box[10] = 3;

  KeyIdList key_ids;
  // TODO(jrummell): This should fail as the 'pssh' box is skipped.
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(box, &key_ids));
  EXPECT_EQ(0u, key_ids.size());
}

TEST_F(CencUtilsTest, LongSize) {
  const uint8_t data[] = {
      0x00, 0x00, 0x00, 0x01,                          // size = 1
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c,  // longsize
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  KeyIdList key_ids;
  EXPECT_TRUE(
      ValidatePsshInput(std::vector<uint8_t>(data, data + arraysize(data))));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(
      std::vector<uint8_t>(data, data + arraysize(data)), &key_ids));
  EXPECT_EQ(2u, key_ids.size());
}

TEST_F(CencUtilsTest, NoSize) {
  const uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00,                          // size = 0
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  KeyIdList key_ids;
  EXPECT_TRUE(
      ValidatePsshInput(std::vector<uint8_t>(data, data + arraysize(data))));
  EXPECT_TRUE(GetKeyIdsForCommonSystemId(
      std::vector<uint8_t>(data, data + arraysize(data)), &key_ids));
  EXPECT_EQ(2u, key_ids.size());
}

TEST_F(CencUtilsTest, HugeSize) {
  const uint8_t data[] = {
      0x00, 0x00, 0x00, 0x01,                          // size = 1
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // longsize = big
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  KeyIdList key_ids;
  EXPECT_FALSE(
      ValidatePsshInput(std::vector<uint8_t>(data, data + arraysize(data))));
  EXPECT_FALSE(GetKeyIdsForCommonSystemId(
      std::vector<uint8_t>(data, data + arraysize(data)), &key_ids));
}

}  // namespace media
