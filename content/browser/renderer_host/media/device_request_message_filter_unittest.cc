// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/media/device_request_message_filter.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace content {

static const std::string kAudioLabel = "audio_label";
static const std::string kVideoLabel = "video_label";

class MockMediaStreamManager : public MediaStreamManager {
 public:
  MockMediaStreamManager() {}

  virtual ~MockMediaStreamManager() {}

  MOCK_METHOD6(EnumerateDevices,
               std::string(MediaStreamRequester* requester,
                           int render_process_id,
                           int render_view_id,
                           int page_request_id,
                           MediaStreamType type,
                           const GURL& security_origin));
  MOCK_METHOD1(StopGeneratedStream, void(const std::string& label));

  std::string DoEnumerateDevices(MediaStreamRequester* requester,
                                 int render_process_id,
                                 int render_view_id,
                                 int page_request_id,
                                 MediaStreamType type,
                                 const GURL& security_origin) {
    if (type == MEDIA_DEVICE_AUDIO_CAPTURE) {
      return kAudioLabel;
    } else {
      return kVideoLabel;
    }
  }
};

class MockDeviceRequestMessageFilter : public DeviceRequestMessageFilter {
 public:
  MockDeviceRequestMessageFilter(MockResourceContext* context,
                                 MockMediaStreamManager* manager)
      : DeviceRequestMessageFilter(context, manager), received_id_(-1) {}
  StreamDeviceInfoArray requested_devices() { return requested_devices_; }
  int received_id() { return received_id_; }

 private:
  virtual ~MockDeviceRequestMessageFilter() {}

  // Override the Send() method to intercept the message that we're sending to
  // the renderer.
  virtual bool Send(IPC::Message* reply_msg) OVERRIDE {
    CHECK(reply_msg);

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockDeviceRequestMessageFilter, *reply_msg)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_GetSourcesACK, SaveDevices)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete reply_msg;
    return true;
  }

  void SaveDevices(int request_id, const StreamDeviceInfoArray& devices) {
    received_id_ = request_id;
    requested_devices_ = devices;
  }

  int received_id_;
  StreamDeviceInfoArray requested_devices_;
};

class DeviceRequestMessageFilterTest : public testing::Test {
 public:
  DeviceRequestMessageFilterTest() : next_device_id_(0) {}

  void RunTest(int number_audio_devices, int number_video_devices) {
    AddAudioDevices(number_audio_devices);
    AddVideoDevices(number_video_devices);
    GURL origin("https://test.com");
    EXPECT_CALL(*media_stream_manager_,
                EnumerateDevices(_, _, _, _, MEDIA_DEVICE_AUDIO_CAPTURE, _))
        .Times(1);
    EXPECT_CALL(*media_stream_manager_,
                EnumerateDevices(_, _, _, _, MEDIA_DEVICE_VIDEO_CAPTURE, _))
        .Times(1);
    // Send message to get devices. Should trigger 2 EnumerateDevice() requests.
    const int kRequestId = 123;
    SendGetSourcesMessage(kRequestId, origin);

    // Run audio callback. Because there's still an outstanding video request,
    // this should not populate |message|.
    FireAudioDeviceCallback();
    EXPECT_EQ(0u, host_->requested_devices().size());

    // After the video device callback is fired, |message| should be populated.
    EXPECT_CALL(*media_stream_manager_, StopGeneratedStream(kAudioLabel))
        .Times(1);
    EXPECT_CALL(*media_stream_manager_, StopGeneratedStream(kVideoLabel))
        .Times(1);
    FireVideoDeviceCallback();
    EXPECT_EQ(static_cast<size_t>(number_audio_devices + number_video_devices),
              host_->requested_devices().size());

    EXPECT_EQ(kRequestId, host_->received_id());
    // Check to make sure no devices have raw ids.
    EXPECT_FALSE(DoesContainRawIds(host_->requested_devices()));

    // Check to make sure every GUID produced matches a raw device id.
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->requested_devices(), origin));
  }

  bool AreLabelsPresent(MediaStreamType type) {
    const StreamDeviceInfoArray& devices = host_->requested_devices();
    for (size_t i = 0; i < devices.size(); i++) {
      if (devices[i].device.type == type && !devices[i].device.name.empty())
        return true;
    }
    return false;
  }

 protected:
  virtual ~DeviceRequestMessageFilterTest() {}

  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_IO));
    io_thread_.reset(
        new TestBrowserThread(BrowserThread::IO, message_loop_.get()));

    media_stream_manager_.reset(new MockMediaStreamManager());
    ON_CALL(*media_stream_manager_, EnumerateDevices(_, _, _, _, _, _))
        .WillByDefault(Invoke(media_stream_manager_.get(),
                              &MockMediaStreamManager::DoEnumerateDevices));

    resource_context_.reset(new MockResourceContext(NULL));
    host_ = new MockDeviceRequestMessageFilter(resource_context_.get(),
                                               media_stream_manager_.get());
  }

  scoped_refptr<MockDeviceRequestMessageFilter> host_;
  scoped_ptr<MockMediaStreamManager> media_stream_manager_;
  scoped_ptr<MockResourceContext> resource_context_;
  StreamDeviceInfoArray physical_audio_devices_;
  StreamDeviceInfoArray physical_video_devices_;
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<TestBrowserThread> io_thread_;

 private:
  void AddAudioDevices(int number_of_devices) {
    for (int i = 0; i < number_of_devices; i++) {
      physical_audio_devices_.push_back(
          StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE,
                           "/dev/audio/" + base::IntToString(next_device_id_),
                           "Audio Device" + base::IntToString(next_device_id_),
                           false));
      next_device_id_++;
    }
  }

  void AddVideoDevices(int number_of_devices) {
    for (int i = 0; i < number_of_devices; i++) {
      physical_video_devices_.push_back(
          StreamDeviceInfo(MEDIA_DEVICE_VIDEO_CAPTURE,
                           "/dev/video/" + base::IntToString(next_device_id_),
                           "Video Device" + base::IntToString(next_device_id_),
                           false));
      next_device_id_++;
    }
  }

  void SendGetSourcesMessage(int request_id, const GURL& origin) {
    // Since we're not actually sending IPC messages, this is a throw-away
    // value.
    bool message_was_ok;
    host_->OnMessageReceived(MediaStreamHostMsg_GetSources(request_id, origin),
                             &message_was_ok);
  }

  void FireAudioDeviceCallback() {
    host_->DevicesEnumerated(kAudioLabel, physical_audio_devices_);
  }

  void FireVideoDeviceCallback() {
    host_->DevicesEnumerated(kVideoLabel, physical_video_devices_);
  }

  bool DoesContainRawIds(const StreamDeviceInfoArray& devices) {
    for (size_t i = 0; i < devices.size(); i++) {
      for (size_t j = 0; j < physical_audio_devices_.size(); ++j) {
        if (physical_audio_devices_[j].device.id == devices[i].device.id)
          return true;
      }
      for (size_t j = 0; j < physical_video_devices_.size(); ++j) {
        if (physical_video_devices_[j].device.id == devices[i].device.id)
          return true;
      }
    }
    return false;
  }

  bool DoesEveryDeviceMapToRawId(const StreamDeviceInfoArray& devices,
                                 const GURL& origin) {
    for (size_t i = 0; i < devices.size(); i++) {
      bool found_match = false;
      for (size_t j = 0; j < physical_audio_devices_.size(); ++j) {
        if (DeviceRequestMessageFilter::DoesRawIdMatchGuid(
                origin,
                devices[i].device.id,
                physical_audio_devices_[j].device.id)) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      for (size_t j = 0; j < physical_video_devices_.size(); ++j) {
        if (DeviceRequestMessageFilter::DoesRawIdMatchGuid(
                origin,
                devices[i].device.id,
                physical_video_devices_[j].device.id)) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      if (!found_match)
        return false;
    }
    return true;
  }

  int next_device_id_;
};

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_AudioAndVideoDevices) {
  // Runs through test with 1 audio and 1 video device.
  RunTest(1, 1);
}

TEST_F(DeviceRequestMessageFilterTest,
       TestGetSources_MultipleAudioAndVideoDevices) {
  // Runs through test with 3 audio devices and 2 video devices.
  RunTest(3, 2);
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_NoVideoDevices) {
  // Runs through test with 4 audio devices and 0 video devices.
  RunTest(4, 0);
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_NoAudioDevices) {
  // Runs through test with 0 audio devices and 3 video devices.
  RunTest(0, 3);
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_NoDevices) {
  // Runs through test with no devices.
  RunTest(0, 0);
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_DenyMicDenyCamera) {
  resource_context_->set_mic_access(false);
  resource_context_->set_camera_access(false);
  RunTest(3, 3);
  EXPECT_FALSE(AreLabelsPresent(MEDIA_DEVICE_AUDIO_CAPTURE));
  EXPECT_FALSE(AreLabelsPresent(MEDIA_DEVICE_VIDEO_CAPTURE));
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_AllowMicDenyCamera) {
  resource_context_->set_mic_access(true);
  resource_context_->set_camera_access(false);
  RunTest(3, 3);
  EXPECT_TRUE(AreLabelsPresent(MEDIA_DEVICE_AUDIO_CAPTURE));
  EXPECT_FALSE(AreLabelsPresent(MEDIA_DEVICE_VIDEO_CAPTURE));
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_DenyMicAllowCamera) {
  resource_context_->set_mic_access(false);
  resource_context_->set_camera_access(true);
  RunTest(3, 3);
  EXPECT_FALSE(AreLabelsPresent(MEDIA_DEVICE_AUDIO_CAPTURE));
  EXPECT_TRUE(AreLabelsPresent(MEDIA_DEVICE_VIDEO_CAPTURE));
}

TEST_F(DeviceRequestMessageFilterTest, TestGetSources_AllowMicAllowCamera) {
  resource_context_->set_mic_access(true);
  resource_context_->set_camera_access(true);
  RunTest(3, 3);
  EXPECT_TRUE(AreLabelsPresent(MEDIA_DEVICE_AUDIO_CAPTURE));
  EXPECT_TRUE(AreLabelsPresent(MEDIA_DEVICE_VIDEO_CAPTURE));
}

};  // namespace content
