// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_service_record.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const char* kAddress = "01:02:03:04:05:06";
static const char* kCustomUuid = "01234567-89ab-cdef-0123-456789abcdef";
static const char* kSerialUuid = "00001101-0000-1000-8000-00805f9b34fb";

}  // namespace

namespace chromeos {

class BluetoothServiceRecordTest : public testing::Test {
 public:
  FilePath GetTestDataFilePath(const char* file) {
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("chromeos");
    path = path.AppendASCII("bluetooth");
    path = path.AppendASCII(file);
    return path;
  }
};

TEST_F(BluetoothServiceRecordTest, RfcommService) {
  std::string xml_data;
  file_util::ReadFileToString(GetTestDataFilePath("rfcomm.xml"), &xml_data);

  BluetoothServiceRecord service_record(kAddress, xml_data);
  EXPECT_EQ(kAddress, service_record.address());
  EXPECT_EQ("Headset Audio Gateway", service_record.name());
  EXPECT_TRUE(service_record.SupportsRfcomm());
  EXPECT_EQ((uint8_t)12, service_record.rfcomm_channel());
  EXPECT_EQ(kCustomUuid, service_record.uuid());
}

TEST_F(BluetoothServiceRecordTest, ShortUuid) {
  std::string xml_data;
  file_util::ReadFileToString(GetTestDataFilePath("short_uuid.xml"), &xml_data);
  BluetoothServiceRecord short_uuid_service_record(kAddress, xml_data);
  EXPECT_EQ(kSerialUuid, short_uuid_service_record.uuid());

  xml_data.clear();
  file_util::ReadFileToString(
      GetTestDataFilePath("medium_uuid.xml"), &xml_data);
  BluetoothServiceRecord medium_uuid_service_record(kAddress, xml_data);
  EXPECT_EQ(kSerialUuid, medium_uuid_service_record.uuid());
}

TEST_F(BluetoothServiceRecordTest, CleanUuid) {
  std::string xml_data;
  file_util::ReadFileToString(GetTestDataFilePath("uppercase_uuid.xml"),
      &xml_data);
  BluetoothServiceRecord service_record(kAddress, xml_data);
  EXPECT_EQ(kCustomUuid, service_record.uuid());

  xml_data.clear();
  file_util::ReadFileToString(GetTestDataFilePath("invalid_uuid.xml"),
      &xml_data);
  BluetoothServiceRecord invalid_service_record(kAddress, xml_data);
  EXPECT_EQ("", invalid_service_record.uuid());
}

}  // namespace chromeos
