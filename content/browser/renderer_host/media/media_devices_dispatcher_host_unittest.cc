// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"

#include <stddef.h>

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/mock_audio_manager.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

namespace {

const int kProcessId = 5;
const int kRenderId = 6;

void PhysicalDevicesEnumerated(base::Closure quit_closure,
                               MediaDeviceEnumeration* out,
                               const MediaDeviceEnumeration& enumeration) {
  *out = enumeration;
  quit_closure.Run();
}

}  // namespace

class MediaDevicesDispatcherHostTest : public testing::Test {
 public:
  MediaDevicesDispatcherHostTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        origin_(GURL("https://test.com")) {
    // Make sure we use fake devices to avoid long delays.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    audio_manager_.reset(
        new media::MockAudioManager(base::ThreadTaskRunnerHandle::Get()));
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));

    MockResourceContext* mock_resource_context =
        static_cast<MockResourceContext*>(
            browser_context_.GetResourceContext());

    host_ = base::MakeUnique<MediaDevicesDispatcherHost>(
        kProcessId, kRenderId, mock_resource_context->GetMediaDeviceIDSalt(),
        media_stream_manager_.get());
  }

  void SetUp() override {
    base::RunLoop run_loop;
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
    devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
    devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&PhysicalDevicesEnumerated, run_loop.QuitClosure(),
                   &physical_devices_));
    run_loop.Run();

    ASSERT_GT(physical_devices_[MEDIA_DEVICE_TYPE_AUDIO_INPUT].size(), 0u);
    ASSERT_GT(physical_devices_[MEDIA_DEVICE_TYPE_VIDEO_INPUT].size(), 0u);
    ASSERT_GT(physical_devices_[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].size(), 0u);
  }

 protected:
  void DevicesEnumerated(
      const base::Closure& closure,
      const std::vector<std::vector<MediaDeviceInfo>>& devices) {
    enumerated_devices_ = devices;
    closure.Run();
  }

  void EnumerateDevicesAndWaitForResult(bool enumerate_audio_input,
                                        bool enumerate_video_input,
                                        bool enumerate_audio_output,
                                        bool permission_override_value = true) {
    MediaDevicesPermissionChecker permission_checker;
    permission_checker.OverridePermissionsForTesting(permission_override_value);
    host_->SetPermissionChecker(permission_checker);
    base::RunLoop run_loop;
    host_->EnumerateDevices(
        enumerate_audio_input, enumerate_video_input, enumerate_audio_output,
        origin_, base::Bind(&MediaDevicesDispatcherHostTest::DevicesEnumerated,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_FALSE(enumerated_devices_.empty());
    if (enumerate_audio_input)
      EXPECT_FALSE(enumerated_devices_[MEDIA_DEVICE_TYPE_AUDIO_INPUT].empty());
    if (enumerate_video_input)
      EXPECT_FALSE(enumerated_devices_[MEDIA_DEVICE_TYPE_VIDEO_INPUT].empty());
    if (enumerate_audio_output)
      EXPECT_FALSE(enumerated_devices_[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].empty());

    EXPECT_FALSE(DoesContainRawIds(enumerated_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(enumerated_devices_, origin_));
  }

  bool DoesContainRawIds(
      const std::vector<std::vector<MediaDeviceInfo>>& enumeration) {
    for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
      for (const auto& device_info : enumeration[i]) {
        for (const auto& raw_device_info : physical_devices_[i]) {
          // Skip default and communications audio devices, whose IDs are not
          // translated.
          if (device_info.device_id ==
                  media::AudioDeviceDescription::kDefaultDeviceId ||
              device_info.device_id ==
                  media::AudioDeviceDescription::kCommunicationsDeviceId) {
            continue;
          }
          if (device_info.device_id == raw_device_info.device_id)
            return true;
        }
      }
    }
    return false;
  }

  bool DoesEveryDeviceMapToRawId(
      const std::vector<std::vector<MediaDeviceInfo>>& enumeration,
      const url::Origin& origin) {
    for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
      for (const auto& device_info : enumeration[i]) {
        bool found_match = false;
        for (const auto& raw_device_info : physical_devices_[i]) {
          if (DoesMediaDeviceIDMatchHMAC(
                  browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
                  origin, device_info.device_id, raw_device_info.device_id)) {
            EXPECT_FALSE(found_match);
            found_match = true;
          }
        }
        if (!found_match)
          return false;
      }
    }
    return true;
  }

  // Returns true if all devices have labels, false otherwise.
  bool DoesContainLabels(
      const std::vector<std::vector<MediaDeviceInfo>>& enumeration) {
    for (const auto& device_info_array : enumeration) {
      for (const auto& device_info : device_info_array) {
        if (device_info.label.empty())
          return false;
      }
    }
    return true;
  }

  // Returns true if no devices have labels, false otherwise.
  bool DoesNotContainLabels(
      const std::vector<std::vector<MediaDeviceInfo>>& enumeration) {
    for (const auto& device_info_array : enumeration) {
      for (const auto& device_info : device_info_array) {
        if (!device_info.label.empty())
          return false;
      }
    }
    return true;
  }

  std::unique_ptr<MediaDevicesDispatcherHost> host_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<media::AudioManager, media::AudioManagerDeleter>
      audio_manager_;
  content::TestBrowserContext browser_context_;
  MediaDeviceEnumeration physical_devices_;
  url::Origin origin_;

  std::vector<std::vector<MediaDeviceInfo>> enumerated_devices_;
};

TEST_F(MediaDevicesDispatcherHostTest, EnumerateAudioInputDevices) {
  EnumerateDevicesAndWaitForResult(true, false, false);
  EXPECT_TRUE(DoesContainLabels(enumerated_devices_));
}

TEST_F(MediaDevicesDispatcherHostTest, EnumerateVideoInputDevices) {
  EnumerateDevicesAndWaitForResult(false, true, false);
  EXPECT_TRUE(DoesContainLabels(enumerated_devices_));
}

TEST_F(MediaDevicesDispatcherHostTest, EnumerateAudioOutputDevices) {
  EnumerateDevicesAndWaitForResult(false, false, true);
  EXPECT_TRUE(DoesContainLabels(enumerated_devices_));
}

TEST_F(MediaDevicesDispatcherHostTest, EnumerateAllDevices) {
  EnumerateDevicesAndWaitForResult(true, true, true);
  EXPECT_TRUE(DoesContainLabels(enumerated_devices_));
}

TEST_F(MediaDevicesDispatcherHostTest, EnumerateAudioInputDevicesNoAccess) {
  EnumerateDevicesAndWaitForResult(true, false, false, false);
  EXPECT_TRUE(DoesNotContainLabels(enumerated_devices_));
}

TEST_F(MediaDevicesDispatcherHostTest, EnumerateVideoInputDevicesNoAccess) {
  EnumerateDevicesAndWaitForResult(false, true, false, false);
  EXPECT_TRUE(DoesNotContainLabels(enumerated_devices_));
}

TEST_F(MediaDevicesDispatcherHostTest, EnumerateAudioOutputDevicesNoAccess) {
  EnumerateDevicesAndWaitForResult(false, false, true, false);
  EXPECT_TRUE(DoesNotContainLabels(enumerated_devices_));
}

TEST_F(MediaDevicesDispatcherHostTest, EnumerateAllDevicesNoAccess) {
  EnumerateDevicesAndWaitForResult(true, true, true, false);
  EXPECT_TRUE(DoesNotContainLabels(enumerated_devices_));
}

};  // namespace content
