// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#import "media/base/mac/avfoundation_glue.h"
#include "media/base/media_switches.h"
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class VideoCaptureDeviceFactoryMacTest : public testing::Test {
  void SetUp() override {
    AVFoundationGlue::InitializeAVFoundation();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableAVFoundation);
  }

 private:
  base::MessageLoop message_loop_;
};

TEST_F(VideoCaptureDeviceFactoryMacTest, ListDevicesAVFoundation) {
  if (!AVFoundationGlue::IsAVFoundationSupported()) {
    DVLOG(1) << "AVFoundation not supported, skipping test.";
    return;
  }
  VideoCaptureDeviceFactoryMac video_capture_device_factory(
      base::ThreadTaskRunnerHandle::Get());

  VideoCaptureDevice::Names names;
  video_capture_device_factory.GetDeviceNames(&names);
  if (!names.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  // There should be no blacklisted devices, i.e. QTKit.
  std::string device_vid;
  for (VideoCaptureDevice::Names::const_iterator it = names.begin();
       it != names.end(); ++it) {
    EXPECT_EQ(it->capture_api_type(), VideoCaptureDevice::Name::AVFOUNDATION);
  }
}

};  // namespace media
