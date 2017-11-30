// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_devices_event_dispatcher.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;

namespace content {
namespace {

class MockMediaDevicesEventSubscriber {
 public:
  MOCK_METHOD2(EventDispatched,
               void(MediaDeviceType, const MediaDeviceInfoArray&));
};

class MockMediaDevicesDispatcherHost
    : public blink::mojom::MediaDevicesDispatcherHost {
 public:
  MockMediaDevicesDispatcherHost() : binding_(this) {}

  MOCK_METHOD2(SubscribeDeviceChangeNotifications,
               void(MediaDeviceType type, uint32_t subscription_id));
  MOCK_METHOD2(UnsubscribeDeviceChangeNotifications,
               void(MediaDeviceType type, uint32_t subscription_id));

  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        EnumerateDevicesCallback) override {
    NOTREACHED();
  }

  void GetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback) override {
    NOTREACHED();
  }

  void GetAudioInputCapabilities(
      GetAudioInputCapabilitiesCallback client_callback) override {
    NOTREACHED();
  }

  void GetAllVideoInputDeviceFormats(
      const std::string&,
      GetAllVideoInputDeviceFormatsCallback) override {
    NOTREACHED();
  }

  void GetAvailableVideoInputDeviceFormats(
      const std::string&,
      GetAvailableVideoInputDeviceFormatsCallback) override {
    NOTREACHED();
  }

  blink::mojom::MediaDevicesDispatcherHostPtr CreateInterfacePtrAndBind() {
    blink::mojom::MediaDevicesDispatcherHostPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

 private:
  mojo::Binding<blink::mojom::MediaDevicesDispatcherHost> binding_;
};

class MediaDevicesEventDispatcherTest : public ::testing::Test {
 public:
  MediaDevicesEventDispatcherTest() {}

  void SetUp() override {
    event_dispatcher_ = MediaDevicesEventDispatcher::GetForRenderFrame(nullptr);
    blink::mojom::MediaDevicesDispatcherHostPtr ptr =
        media_devices_dispatcher_.CreateInterfacePtrAndBind();
    event_dispatcher_->SetMediaDevicesDispatcherForTesting(std::move(ptr));
  }

  void TearDown() override { event_dispatcher_->OnDestruct(); }

 protected:
  base::MessageLoop message_loop_;
  base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher_;
  MockMediaDevicesDispatcherHost media_devices_dispatcher_;
};

}  // namespace

TEST_F(MediaDevicesEventDispatcherTest, SubscribeUnsubscribeSingleType) {
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    MediaDeviceType type = static_cast<MediaDeviceType>(i);
    MediaDeviceInfoArray device_infos;
    MockMediaDevicesEventSubscriber subscriber1, subscriber2;
    EXPECT_CALL(media_devices_dispatcher_,
                SubscribeDeviceChangeNotifications(type, _))
        .Times(2);
    auto subscription_id1 =
        event_dispatcher_->SubscribeDeviceChangeNotifications(
            type, base::Bind(&MockMediaDevicesEventSubscriber::EventDispatched,
                             base::Unretained(&subscriber1)));
    auto subscription_id2 =
        event_dispatcher_->SubscribeDeviceChangeNotifications(
            type, base::Bind(&MockMediaDevicesEventSubscriber::EventDispatched,
                             base::Unretained(&subscriber2)));

    // Simulate a device change.
    EXPECT_CALL(subscriber1, EventDispatched(type, _));
    EXPECT_CALL(subscriber2, EventDispatched(type, _));
    event_dispatcher_->DispatchDevicesChangedEvent(type, device_infos);
    base::RunLoop().RunUntilIdle();

    // Unsubscribe one of the subscribers.
    EXPECT_CALL(media_devices_dispatcher_,
                UnsubscribeDeviceChangeNotifications(type, subscription_id1));
    event_dispatcher_->UnsubscribeDeviceChangeNotifications(type,
                                                            subscription_id1);

    // Simulate another device change.
    EXPECT_CALL(subscriber1, EventDispatched(_, _)).Times(0);
    EXPECT_CALL(subscriber2, EventDispatched(type, _));
    event_dispatcher_->DispatchDevicesChangedEvent(type, device_infos);
    base::RunLoop().RunUntilIdle();

    // Unsubscribe the other subscriber.
    EXPECT_CALL(media_devices_dispatcher_,
                UnsubscribeDeviceChangeNotifications(type, subscription_id2));
    event_dispatcher_->UnsubscribeDeviceChangeNotifications(type,
                                                            subscription_id2);
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(MediaDevicesEventDispatcherTest, SubscribeUnsubscribeAllTypes) {
  MediaDeviceInfoArray device_infos;
  MockMediaDevicesEventSubscriber subscriber1, subscriber2;
  EXPECT_CALL(media_devices_dispatcher_, SubscribeDeviceChangeNotifications(
                                             MEDIA_DEVICE_TYPE_AUDIO_INPUT, _))
      .Times(2);
  EXPECT_CALL(media_devices_dispatcher_, SubscribeDeviceChangeNotifications(
                                             MEDIA_DEVICE_TYPE_VIDEO_INPUT, _))
      .Times(2);
  EXPECT_CALL(media_devices_dispatcher_, SubscribeDeviceChangeNotifications(
                                             MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, _))
      .Times(2);
  auto subscription_list_1 =
      event_dispatcher_->SubscribeDeviceChangeNotifications(
          base::Bind(&MockMediaDevicesEventSubscriber::EventDispatched,
                     base::Unretained(&subscriber1)));
  auto subscription_list_2 =
      event_dispatcher_->SubscribeDeviceChangeNotifications(
          base::Bind(&MockMediaDevicesEventSubscriber::EventDispatched,
                     base::Unretained(&subscriber2)));

  // Simulate a device changes for all types.
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    MediaDeviceType type = static_cast<MediaDeviceType>(i);
    EXPECT_CALL(subscriber1, EventDispatched(type, _));
    EXPECT_CALL(subscriber2, EventDispatched(type, _));
    event_dispatcher_->DispatchDevicesChangedEvent(type, device_infos);
    base::RunLoop().RunUntilIdle();
  }

  // Unsubscribe one of the subscribers.
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    MediaDeviceType type = static_cast<MediaDeviceType>(i);
    EXPECT_CALL(
        media_devices_dispatcher_,
        UnsubscribeDeviceChangeNotifications(type, subscription_list_1[type]));
  }
  event_dispatcher_->UnsubscribeDeviceChangeNotifications(subscription_list_1);
  base::RunLoop().RunUntilIdle();

  // Simulate more device changes.
  EXPECT_CALL(subscriber1, EventDispatched(_, _)).Times(0);
  EXPECT_CALL(subscriber2, EventDispatched(_, _)).Times(NUM_MEDIA_DEVICE_TYPES);
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    MediaDeviceType type = static_cast<MediaDeviceType>(i);
    event_dispatcher_->DispatchDevicesChangedEvent(type, device_infos);
    base::RunLoop().RunUntilIdle();
  }

  // Unsubscribe the other subscriber.
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    MediaDeviceType type = static_cast<MediaDeviceType>(i);
    EXPECT_CALL(
        media_devices_dispatcher_,
        UnsubscribeDeviceChangeNotifications(type, subscription_list_2[type]));
  }
  event_dispatcher_->UnsubscribeDeviceChangeNotifications(subscription_list_2);
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
