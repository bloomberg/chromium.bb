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
static const char* kUuid = "00001101-0000-1000-8000-0123456789ab";

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
  EXPECT_EQ(kUuid, service_record.uuid());
}

}  // namespace chromeos
