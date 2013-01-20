// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestNoRfcommSdpBytes[] =
    "35510900000a00010001090001350319110a09000435103506190100090019350619001909"
    "010209000535031910020900093508350619110d090102090100250c417564696f20536f75"
    "726365090311090001";
const int kTestNoRfcommSdpBytesSize = sizeof(kTestNoRfcommSdpBytes) / 2;
const char kTestNoRfcommSdpUuid[] = "0000110a-0000-1000-8000-00805f9b34fb";

const char kTestRfcommSdpBytes[] =
    "354b0900000a000100030900013506191112191203090004350c3503190100350519000308"
    "0b090005350319100209000935083506191108090100090100250d566f6963652047617465"
    "776179";
const int kTestRfcommSdpBytesSize = sizeof(kTestRfcommSdpBytes) / 2;
const char kTestRfcommSdpUuid[] = "00001112-0000-1000-8000-00805f9b34fb";
const int kTestRfcommChannel = 11;

}  // namespace

namespace device {

class BluetoothServiceRecordWinTest : public testing::Test {
 protected:
  void ConvertSdpBytes(const char* sdp_hex_char, uint8* sdp_bytes_array) {
    std::vector<uint8> sdp_bytes_vector;
    base::HexStringToBytes(sdp_hex_char, &sdp_bytes_vector);
    std::copy(sdp_bytes_vector.begin(),
              sdp_bytes_vector.end(),
              sdp_bytes_array);
  }
};

TEST_F(BluetoothServiceRecordWinTest, NoRfcommSdp) {
  uint8 sdp_bytes_array[kTestNoRfcommSdpBytesSize];
  ConvertSdpBytes(kTestNoRfcommSdpBytes, sdp_bytes_array);
  BluetoothServiceRecordWin service_record("NoRfcommSdp",
                                           "Service Address",
                                           kTestNoRfcommSdpBytesSize,
                                           sdp_bytes_array);
  EXPECT_STREQ(kTestNoRfcommSdpUuid, service_record.uuid().c_str());
  EXPECT_FALSE(service_record.SupportsRfcomm());
}


TEST_F(BluetoothServiceRecordWinTest, RfcommSdp) {
  uint8 sdp_bytes_array[kTestRfcommSdpBytesSize];
  ConvertSdpBytes(kTestRfcommSdpBytes, sdp_bytes_array);
  BluetoothServiceRecordWin service_record("RfcommSdp",
                                           "Service Address",
                                           kTestRfcommSdpBytesSize,
                                           sdp_bytes_array);
  EXPECT_STREQ(kTestRfcommSdpUuid, service_record.uuid().c_str());
  EXPECT_TRUE(service_record.SupportsRfcomm());
  EXPECT_EQ(kTestRfcommChannel, service_record.rfcomm_channel());
}

}  // namespace device