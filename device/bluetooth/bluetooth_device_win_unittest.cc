// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_service_record.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDeviceName[] = "Device";
const char kDeviceAddress[] = "device address";

const char kTestAudioSdpName[] = "Audio";
const char kTestAudioSdpAddress[] = "01:02:03:0A:10:A0";
const char kTestAudioSdpBytes[] =
    "35510900000a00010001090001350319110a09000435103506190100090019350619001909"
    "010209000535031910020900093508350619110d090102090100250c417564696f20536f75"
    "726365090311090001";
const char kTestAudioSdpUuid[] = "0000110a-0000-1000-8000-00805f9b34fb";

const char kTestVideoSdpName[] = "Video";
const char kTestVideoSdpAddress[] = "A0:10:0A:03:02:01";
const char kTestVideoSdpBytes[] =
    "354b0900000a000100030900013506191112191203090004350c3503190100350519000308"
    "0b090005350319100209000935083506191108090100090100250d566f6963652047617465"
    "776179";
const char kTestVideoSdpUuid[] = "00001112-0000-1000-8000-00805f9b34fb";

}  // namespace

namespace device {

class BluetoothDeviceWinTest : public testing::Test {
 public:
  BluetoothDeviceWinTest() {
    BluetoothTaskManagerWin::DeviceState device_state;
    device_state.name = kDeviceName;
    device_state.address = kDeviceAddress;

    BluetoothTaskManagerWin::ServiceRecordState* audio_state =
        new BluetoothTaskManagerWin::ServiceRecordState();
    audio_state->name = kTestAudioSdpName;
    audio_state->address = kTestAudioSdpAddress;
    base::HexStringToBytes(kTestAudioSdpBytes, &audio_state->sdp_bytes);
    device_state.service_record_states.push_back(audio_state);

    BluetoothTaskManagerWin::ServiceRecordState* video_state =
        new BluetoothTaskManagerWin::ServiceRecordState();
    video_state->name = kTestVideoSdpName;
    video_state->address = kTestVideoSdpAddress;
    base::HexStringToBytes(kTestVideoSdpBytes, &video_state->sdp_bytes);
    device_state.service_record_states.push_back(video_state);

    device_.reset(new BluetoothDeviceWin(device_state));
  }

 protected:
  scoped_ptr<BluetoothDevice> device_;
  scoped_ptr<BluetoothDevice> empty_device_;
};

TEST_F(BluetoothDeviceWinTest, GetUUIDs) {
  BluetoothDevice::UUIDList uuids = device_->GetUUIDs();

  EXPECT_EQ(2, uuids.size());
  EXPECT_STREQ(kTestAudioSdpUuid, uuids[0].c_str());
  EXPECT_STREQ(kTestVideoSdpUuid, uuids[1].c_str());

  uuids = empty_device_->GetUUIDs();
  EXPECT_EQ(0, uuids.size());
}

}  // namespace device
