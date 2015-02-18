// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/bluetooth_media_client.h"
#include "chromeos/dbus/bluetooth_media_endpoint_service_provider.h"
#include "chromeos/dbus/bluetooth_media_transport_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_media_client.h"
#include "chromeos/dbus/fake_bluetooth_media_endpoint_service_provider.h"
#include "chromeos/dbus/fake_bluetooth_media_transport_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_audio_sink.h"
#include "device/bluetooth/bluetooth_audio_sink_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

using dbus::ObjectPath;
using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothAudioSink;

namespace chromeos {

class TestAudioSinkObserver : public BluetoothAudioSink::Observer {
 public:
  explicit TestAudioSinkObserver(scoped_refptr<BluetoothAudioSink> audio_sink)
      : state_changed_count_(0),
        volume_changed_count_(0),
        state_(audio_sink->GetState()),
        audio_sink_(audio_sink) {
    audio_sink_->AddObserver(this);
  }

  ~TestAudioSinkObserver() override { audio_sink_->RemoveObserver(this); }

  void BluetoothAudioSinkStateChanged(
      BluetoothAudioSink* audio_sink,
      BluetoothAudioSink::State state) override {
    ++state_changed_count_;
  }

  void BluetoothAudioSinkVolumeChanged(BluetoothAudioSink* audio_sink,
                                       uint16_t volume) override {
    ++volume_changed_count_;
  }

  int state_changed_count_;
  int volume_changed_count_;
  BluetoothAudioSink::State state_;

 private:
  scoped_refptr<BluetoothAudioSink> audio_sink_;
};

class BluetoothAudioSinkChromeOSTest : public testing::Test {
 public:
  void SetUp() override {
    DBusThreadManager::Initialize();

    callback_count_ = 0;
    error_callback_count_ = 0;

    // Initiates Delegate::TransportProperties with default values.
    properties_.device =
        ObjectPath(FakeBluetoothMediaTransportClient::kTransportDevicePath);
    properties_.uuid = BluetoothMediaClient::kBluetoothAudioSinkUUID;
    properties_.codec = FakeBluetoothMediaTransportClient::kTransportCodec;
    properties_.configuration =
        FakeBluetoothMediaTransportClient::kTransportConfiguration;
    properties_.state = BluetoothMediaTransportClient::kStateIdle;
    properties_.delay.reset(
        new uint16_t(FakeBluetoothMediaTransportClient::kTransportDelay));
    properties_.volume.reset(
        new uint16_t(FakeBluetoothMediaTransportClient::kTransportVolume));

    GetAdapter();
  }

  void TearDown() override {
    callback_count_ = 0;
    error_callback_count_ = 0;

    // The adapter should outlive audio sink.
    audio_sink_ = nullptr;
    adapter_ = nullptr;
    DBusThreadManager::Shutdown();
  }

  // Gets the existing Bluetooth adapter.
  void GetAdapter() {
    BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothAudioSinkChromeOSTest::GetAdapterCallback,
                   base::Unretained(this)));
  }

  // Called whenever BluetoothAdapter is retrieved successfully.
  void GetAdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;

    ASSERT_NE(adapter_.get(), nullptr);
    ASSERT_TRUE(adapter_->IsInitialized());
    adapter_->SetPowered(
        true,
        base::Bind(&BluetoothAudioSinkChromeOSTest::Callback,
                   base::Unretained(this)),
        base::Bind(&BluetoothAudioSinkChromeOSTest::ErrorCallback,
                   base::Unretained(this)));
    ASSERT_TRUE(adapter_->IsPresent());
    ASSERT_TRUE(adapter_->IsPowered());
    EXPECT_EQ(callback_count_, 1);
    EXPECT_EQ(error_callback_count_, 0);

    // Resets callback_count_.
    --callback_count_;
  }

  // Registers BluetoothAudioSinkChromeOS with default codec and capabilities.
  void GetAudioSink() {
    // Sets up valid codec and capabilities.
    BluetoothAudioSink::Options options;
    ASSERT_EQ(options.codec, 0x00);
    ASSERT_EQ(options.capabilities,
              std::vector<uint8_t>({0x3f, 0xff, 0x12, 0x35}));

    // Registers |audio_sink_| with valid codec and capabilities
    adapter_->RegisterAudioSink(
        options,
        base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterCallback,
                   base::Unretained(this)),
        base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterErrorCallback,
                   base::Unretained(this)));

    EXPECT_EQ(callback_count_, 1);
    EXPECT_EQ(error_callback_count_, 0);
  }

  // Called whenever RegisterAudioSink is completed successfully.
  void RegisterCallback(
      scoped_refptr<BluetoothAudioSink> audio_sink) {
    ++callback_count_;
    audio_sink_ = audio_sink;
    ASSERT_NE(audio_sink_.get(), nullptr);
    ASSERT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  }

  // Called whenever RegisterAudioSink failed.
  void RegisterErrorCallback(BluetoothAudioSink::ErrorCode error_code) {
    ++error_callback_count_;
    EXPECT_EQ(error_code, BluetoothAudioSink::ERROR_NOT_REGISTERED);
  }

  // Called whenever there capabilities are returned from SelectConfiguration.
  void SelectConfigurationCallback(const std::vector<uint8_t>& capabilities) {
    ++callback_count_;

    // |capabilities| should be the same as the capabilities for registering an
    // audio sink in GetAudioSink().
    EXPECT_EQ(capabilities, std::vector<uint8_t>({0x3f, 0xff, 0x12, 0x35}));
  }

  // Generic callbacks.
  void Callback() {
    ++callback_count_;
  }

  void ErrorCallback() {
    ++error_callback_count_;
  }

 protected:
  int callback_count_;
  int error_callback_count_;

  base::MessageLoop message_loop_;

  scoped_refptr<BluetoothAdapter> adapter_;
  scoped_refptr<BluetoothAudioSink> audio_sink_;

  // The default property set used while calling SetConfiguration on a media
  // endpoint object.
  BluetoothMediaEndpointServiceProvider::Delegate::TransportProperties
      properties_;
};

TEST_F(BluetoothAudioSinkChromeOSTest, RegisterSucceeded) {
  GetAudioSink();

  // Adds an observer for |audio_sink_|.
  TestAudioSinkObserver observer(audio_sink_);
  EXPECT_EQ(observer.state_changed_count_, 0);
  EXPECT_EQ(observer.volume_changed_count_, 0);
}

TEST_F(BluetoothAudioSinkChromeOSTest, RegisterFailedWithInvalidOptions) {
  // Sets options with an invalid codec and valid capabilities.
  BluetoothAudioSink::Options options;
  options.codec = 0xff;
  options.capabilities = std::vector<uint8_t>({0x3f, 0xff, 0x12, 0x35});

  adapter_->RegisterAudioSink(
      options,
      base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(callback_count_, 0);
  EXPECT_EQ(error_callback_count_, 1);

  // Sets options with a valid codec and invalid capabilities.
  options.codec = 0x00;
  options.capabilities.clear();
  adapter_->RegisterAudioSink(
      options,
      base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(callback_count_, 0);
  EXPECT_EQ(error_callback_count_, 2);
}

TEST_F(BluetoothAudioSinkChromeOSTest, SelectConfiguration) {
  GetAudioSink();

  // Adds an observer for |audio_sink_|.
  TestAudioSinkObserver observer(audio_sink_);
  EXPECT_EQ(observer.state_changed_count_, 0);
  EXPECT_EQ(observer.volume_changed_count_, 0);

  // Simulates calling SelectConfiguration on the media endpoint object owned by
  // |audio_sink_| with some fake capabilities.
  BluetoothAudioSinkChromeOS* audio_sink_chromeos =
      static_cast<BluetoothAudioSinkChromeOS*>(audio_sink_.get());
  FakeBluetoothMediaEndpointServiceProvider* media_endpoint =
      static_cast<FakeBluetoothMediaEndpointServiceProvider*>(
          audio_sink_chromeos->GetEndpointServiceProvider());
  ASSERT_NE(media_endpoint, nullptr);

  media_endpoint->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer.state_changed_count_, 0);
  EXPECT_EQ(observer.volume_changed_count_, 0);
}

TEST_F(BluetoothAudioSinkChromeOSTest, SetConfiguration) {
  GetAudioSink();

  // Adds an observer for |audio_sink_|.
  TestAudioSinkObserver observer(audio_sink_);
  EXPECT_EQ(observer.state_changed_count_, 0);
  EXPECT_EQ(observer.volume_changed_count_, 0);

  // Simulates calling SetConfiguration on the media endpoint object owned by
  // |audio_sink_| with a fake transport path and a
  // Delegate::TransportProperties structure.
  BluetoothAudioSinkChromeOS* audio_sink_chromeos =
      static_cast<BluetoothAudioSinkChromeOS*>(audio_sink_.get());
  FakeBluetoothMediaEndpointServiceProvider* media_endpoint =
      static_cast<FakeBluetoothMediaEndpointServiceProvider*>(
          audio_sink_chromeos->GetEndpointServiceProvider());
  ASSERT_NE(media_endpoint, nullptr);

  media_endpoint->SetConfiguration(
      ObjectPath(FakeBluetoothMediaTransportClient::kTransportPath),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 1);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer.state_changed_count_, 1);
  EXPECT_EQ(observer.volume_changed_count_, 1);
}

TEST_F(BluetoothAudioSinkChromeOSTest, SetConfigurationWithUnexpectedState) {
  GetAudioSink();

  // Adds an observer for |audio_sink_|.
  TestAudioSinkObserver observer(audio_sink_);
  EXPECT_EQ(observer.state_changed_count_, 0);
  EXPECT_EQ(observer.volume_changed_count_, 0);

  // Simulates calling SetConfiguration on the media endpoint object owned by
  // |audio_sink_| with a fake transport path and a
  // Delegate::TransportProperties structure.
  BluetoothAudioSinkChromeOS* audio_sink_chromeos =
      static_cast<BluetoothAudioSinkChromeOS*>(audio_sink_.get());
  FakeBluetoothMediaEndpointServiceProvider* media_endpoint =
      static_cast<FakeBluetoothMediaEndpointServiceProvider*>(
          audio_sink_chromeos->GetEndpointServiceProvider());
  ASSERT_NE(media_endpoint, nullptr);

  // Set state of Delegate::TransportProperties with an unexpected value.
  properties_.state = "active";

  media_endpoint->SetConfiguration(
      ObjectPath(FakeBluetoothMediaTransportClient::kTransportPath),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 1);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer.state_changed_count_, 0);
  EXPECT_EQ(observer.volume_changed_count_, 0);
}

// Checks if the observer is notified while the media object is
// removed(invisible). Once the media object is removed, the audio sink is no
// longer valid.
TEST_F(BluetoothAudioSinkChromeOSTest, ObserverNotifiedWhenMediaRemoved) {
  GetAudioSink();

  // Adds an observer for |audio_sink_|.
  TestAudioSinkObserver observer(audio_sink_);
  EXPECT_EQ(observer.state_changed_count_, 0);
  EXPECT_EQ(observer.volume_changed_count_, 0);

  // Gets the media object and makes it invisible to see if the state of the
  // audio sink changes accordingly.
  FakeBluetoothMediaClient* media = static_cast<FakeBluetoothMediaClient*>(
      DBusThreadManager::Get()->GetBluetoothMediaClient());
  media->SetVisible(false);

  BluetoothAudioSinkChromeOS* audio_sink_chromeos =
      static_cast<BluetoothAudioSinkChromeOS*>(audio_sink_.get());
  FakeBluetoothMediaEndpointServiceProvider* media_endpoint =
      static_cast<FakeBluetoothMediaEndpointServiceProvider*>(
          audio_sink_chromeos->GetEndpointServiceProvider());

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_INVALID);
  EXPECT_EQ(media_endpoint, nullptr);
}

// Checks if the observer is notified while the media transport is
// removed(invisible). Once the media transport object is removed, the audio
// sink is disconnected.
TEST_F(BluetoothAudioSinkChromeOSTest, ObserverNotifiedWhenTransportRemoved) {
  GetAudioSink();

  // Adds an observer for |audio_sink_|.
  TestAudioSinkObserver observer(audio_sink_);
  EXPECT_EQ(observer.state_changed_count_, 0);
  EXPECT_EQ(observer.volume_changed_count_, 0);

  // Simulates calling SetConfiguration on the media endpoint object owned by
  // |audio_sink_| with a fake transport path and a
  // Delegate::TransportProperties structure.
  BluetoothAudioSinkChromeOS* audio_sink_chromeos =
      static_cast<BluetoothAudioSinkChromeOS*>(audio_sink_.get());
  FakeBluetoothMediaEndpointServiceProvider* media_endpoint =
      static_cast<FakeBluetoothMediaEndpointServiceProvider*>(
          audio_sink_chromeos->GetEndpointServiceProvider());
  ASSERT_NE(media_endpoint, nullptr);

  media_endpoint->SetConfiguration(
      ObjectPath(FakeBluetoothMediaTransportClient::kTransportPath),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 1);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer.state_changed_count_, 1);
  EXPECT_EQ(observer.volume_changed_count_, 1);

  // Gets the media transport object and makes it invalid to see if the state
  // of the audio sink changes accordingly.
  FakeBluetoothMediaTransportClient* transport =
      static_cast<FakeBluetoothMediaTransportClient*>(
          DBusThreadManager::Get()->GetBluetoothMediaTransportClient());

  transport->SetValid(media_endpoint->object_path(), false);
  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_NE(media_endpoint, nullptr);
}

}  // namespace chromeos
