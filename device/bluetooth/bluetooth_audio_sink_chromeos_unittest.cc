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
        total_read_(0),
        state_(audio_sink->GetState()),
        audio_sink_(audio_sink) {
    audio_sink_->AddObserver(this);
  }

  ~TestAudioSinkObserver() override { audio_sink_->RemoveObserver(this); }

  void BluetoothAudioSinkStateChanged(
      BluetoothAudioSink* audio_sink,
      BluetoothAudioSink::State state) override {
    if (state == BluetoothAudioSink::STATE_IDLE)
      total_read_ = 0;

    ++state_changed_count_;
  }

  void BluetoothAudioSinkVolumeChanged(BluetoothAudioSink* audio_sink,
                                       uint16_t volume) override {
    ++volume_changed_count_;
  }

  void BluetoothAudioSinkDataAvailable(BluetoothAudioSink* audio_sink,
                                       char* data,
                                       size_t size) override {
    total_read_ += size;
    data_.clear();
    data_.insert(data_.begin(), data, data + size);
  }

  int state_changed_count_;
  int volume_changed_count_;
  int data_available_count_;
  size_t total_read_;
  std::vector<char> data_;
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

    fake_media_ = static_cast<FakeBluetoothMediaClient*>(
        DBusThreadManager::Get()->GetBluetoothMediaClient());
    fake_transport_ = static_cast<FakeBluetoothMediaTransportClient*>(
        DBusThreadManager::Get()->GetBluetoothMediaTransportClient());

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
    observer_.reset();

    fake_media_->SetVisible(true);

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
  // If the audio sink is retrieved successfully, the state changes to
  // STATE_DISCONNECTED.
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

    observer_.reset(new TestAudioSinkObserver(audio_sink_));
    EXPECT_EQ(callback_count_, 1);
    EXPECT_EQ(error_callback_count_, 0);
    EXPECT_EQ(observer_->state_changed_count_, 0);
    EXPECT_EQ(observer_->volume_changed_count_, 0);
  }

  void GetFakeMediaEndpoint() {
    BluetoothAudioSinkChromeOS* audio_sink_chromeos =
        static_cast<BluetoothAudioSinkChromeOS*>(audio_sink_.get());
    ASSERT_NE(audio_sink_chromeos, nullptr);

    media_endpoint_ = static_cast<FakeBluetoothMediaEndpointServiceProvider*>(
        audio_sink_chromeos->GetEndpointServiceProvider());
  }

  // Called whenever RegisterAudioSink is completed successfully.
  void RegisterCallback(
      scoped_refptr<BluetoothAudioSink> audio_sink) {
    ++callback_count_;
    audio_sink_ = audio_sink;

    GetFakeMediaEndpoint();
    ASSERT_NE(media_endpoint_, nullptr);
    fake_media_->SetEndpointRegistered(media_endpoint_, true);

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

  void UnregisterErrorCallback(BluetoothAudioSink::ErrorCode error_code) {
    ++error_callback_count_;
    EXPECT_EQ(error_code, BluetoothAudioSink::ERROR_NOT_UNREGISTERED);
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

  base::MessageLoopForIO message_loop_;

  FakeBluetoothMediaClient* fake_media_;
  FakeBluetoothMediaTransportClient* fake_transport_;
  FakeBluetoothMediaEndpointServiceProvider* media_endpoint_;
  scoped_ptr<TestAudioSinkObserver> observer_;
  scoped_refptr<BluetoothAdapter> adapter_;
  scoped_refptr<BluetoothAudioSink> audio_sink_;

  // The default property set used while calling SetConfiguration on a media
  // endpoint object.
  BluetoothMediaEndpointServiceProvider::Delegate::TransportProperties
      properties_;
};

TEST_F(BluetoothAudioSinkChromeOSTest, RegisterSucceeded) {
  GetAudioSink();
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

  // Simulates calling SelectConfiguration on the media endpoint object owned by
  // |audio_sink_| with some fake capabilities.
  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);
}

TEST_F(BluetoothAudioSinkChromeOSTest, SetConfiguration) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  // Simulates calling SetConfiguration on the media endpoint object owned by
  // |audio_sink_| with a fake transport path and a
  // Delegate::TransportProperties structure.
  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);
}

TEST_F(BluetoothAudioSinkChromeOSTest, SetConfigurationWithUnexpectedState) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  // Set state of Delegate::TransportProperties with an unexpected value.
  properties_.state = "pending";

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);
}

// Checks if the observer is notified on media-removed event when the state of
// |audio_sink_| is STATE_DISCONNECTED. Once the media object is removed, the
// audio sink is no longer valid.
TEST_F(BluetoothAudioSinkChromeOSTest, MediaRemovedDuringDisconnectedState) {
  GetAudioSink();

  // Gets the media object and makes it invisible to see if the state of the
  // audio sink changes accordingly.
  fake_media_->SetVisible(false);

  GetFakeMediaEndpoint();

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_INVALID);
  EXPECT_EQ(media_endpoint_, nullptr);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 0);
}

// Checks if the observer is  notified on media-removed event when the state of
// |audio_sink_| is STATE_IDLE. Once the media object is removed, the audio sink
// is no longer valid.
TEST_F(BluetoothAudioSinkChromeOSTest, MediaRemovedDuringIdleState) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  // Gets the media object and makes it invisible to see if the state of the
  // audio sink changes accordingly.
  fake_media_->SetVisible(false);

  GetFakeMediaEndpoint();

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_INVALID);
  EXPECT_EQ(media_endpoint_, nullptr);

  // The state becomes disconnted and then invalid, since the removal of
  // transport object should happend before media becomes invisible.
  // State: STATE_IDLE -> STATE_DISCONNECTED -> STATE_INVALID
  EXPECT_EQ(observer_->state_changed_count_, 3);
  EXPECT_EQ(observer_->volume_changed_count_, 2);
}

TEST_F(BluetoothAudioSinkChromeOSTest, MediaRemovedDuringActiveState) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  fake_transport_->SetState(media_endpoint_->object_path(), "pending");

  message_loop_.RunUntilIdle();

  // Acquire is called when the state of |audio_sink_| becomes STATE_PENDING,
  // and Acquire will trigger state change. Therefore, the state will be
  // STATE_ACTIVE right after STATE_PENDING.
  // State: STATE_IDLE -> STATE_PENDING -> STATE_ACTIVE
  EXPECT_EQ(observer_->state_changed_count_, 3);

  // Gets the media object and makes it invisible to see if the state of the
  // audio sink changes accordingly.
  fake_media_->SetVisible(false);

  GetFakeMediaEndpoint();

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_INVALID);
  EXPECT_EQ(media_endpoint_, nullptr);

  // The state becomes disconnted and then invalid, since the removal of
  // transport object should happend before media becomes invisible.
  // State: STATE_ACTIVE -> STATE_DISCONNECTED -> STATE_INVALID
  EXPECT_EQ(observer_->state_changed_count_, 5);
  EXPECT_EQ(observer_->volume_changed_count_, 2);
}

// Checks if the observer is notified on transport-removed event when the state
// of |audio_sink_| is STATE_IDEL. Once the media transport object is removed,
// the audio sink is disconnected.
TEST_F(BluetoothAudioSinkChromeOSTest, TransportRemovedDuringIdleState) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  // Makes the transport object invalid to see if the state of the audio sink
  // changes accordingly.
  fake_transport_->SetValid(media_endpoint_, false);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_NE(media_endpoint_, nullptr);
  EXPECT_EQ(observer_->state_changed_count_, 2);
  EXPECT_EQ(observer_->volume_changed_count_, 2);
}

TEST_F(BluetoothAudioSinkChromeOSTest, TransportRemovedDuringActiveState) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  fake_transport_->SetState(media_endpoint_->object_path(), "pending");

  message_loop_.RunUntilIdle();

  // Acquire is called when the state of |audio_sink_| becomes STATE_PENDING,
  // and Acquire will trigger state change. Therefore, the state will be
  // STATE_ACTIVE right after STATE_PENDING.
  // State: STATE_IDLE -> STATE_PENDING -> STATE_ACTIVE
  EXPECT_EQ(observer_->state_changed_count_, 3);

  // Makes the transport object invalid to see if the state of the audio sink
  // changes accordingly.
  fake_transport_->SetValid(media_endpoint_, false);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_NE(media_endpoint_, nullptr);
  EXPECT_EQ(observer_->state_changed_count_, 4);
  EXPECT_EQ(observer_->volume_changed_count_, 2);
}

TEST_F(BluetoothAudioSinkChromeOSTest,
       AdapterPoweredChangedDuringDisconnectedState) {
  GetAudioSink();

  adapter_->SetPowered(
      false,
      base::Bind(&BluetoothAudioSinkChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothAudioSinkChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 3);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);
}

TEST_F(BluetoothAudioSinkChromeOSTest, AdapterPoweredChangedDuringIdleState) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  adapter_->SetPowered(
      false,
      base::Bind(&BluetoothAudioSinkChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  GetFakeMediaEndpoint();

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_NE(media_endpoint_, nullptr);
  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 3);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 2);
  EXPECT_EQ(observer_->volume_changed_count_, 2);
}

TEST_F(BluetoothAudioSinkChromeOSTest,
       UnregisterAudioSinkDuringDisconnectedState) {
  GetAudioSink();

  audio_sink_->Unregister(
      base::Bind(&BluetoothAudioSinkChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::UnregisterErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_INVALID);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 0);
}

TEST_F(BluetoothAudioSinkChromeOSTest, UnregisterAudioSinkDuringIdleState) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  audio_sink_->Unregister(
      base::Bind(&BluetoothAudioSinkChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::UnregisterErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_INVALID);
  EXPECT_EQ(callback_count_, 3);
  EXPECT_EQ(error_callback_count_, 0);

  // The state becomes disconnted and then invalid, since the removal of
  // transport object should happend before the unregistration of endpoint.
  // State: STATE_IDLE -> STATE_DISCONNECTED -> STATE_INVALID
  EXPECT_EQ(observer_->state_changed_count_, 3);
  EXPECT_EQ(observer_->volume_changed_count_, 2);
}

TEST_F(BluetoothAudioSinkChromeOSTest, UnregisterAudioSinkDuringActiveState) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  fake_transport_->SetState(media_endpoint_->object_path(), "pending");

  message_loop_.RunUntilIdle();

  // Acquire is called when the state of |audio_sink_| becomes STATE_PENDING,
  // and Acquire will trigger state change. Therefore, the state will be
  // STATE_ACTIVE right after STATE_PENDING.
  // State: STATE_IDLE -> STATE_PENDING -> STATE_ACTIVE
  EXPECT_EQ(observer_->state_changed_count_, 3);

  audio_sink_->Unregister(
      base::Bind(&BluetoothAudioSinkChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::UnregisterErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_INVALID);
  EXPECT_EQ(callback_count_, 3);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 5);
  EXPECT_EQ(observer_->volume_changed_count_, 2);
}

TEST_F(BluetoothAudioSinkChromeOSTest, StateChanged) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  // Changes the current state of transport to pending.
  fake_transport_->SetState(media_endpoint_->object_path(), "pending");

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_PENDING);
  EXPECT_EQ(observer_->state_changed_count_, 3);
  EXPECT_EQ(observer_->volume_changed_count_, 1);
}

TEST_F(BluetoothAudioSinkChromeOSTest, VolumeChanged) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  // |kTransportVolume| is the initial volume of the transport, and this
  // value is propagated to the audio sink via SetConfiguration.
  EXPECT_EQ(audio_sink_->GetVolume(),
            FakeBluetoothMediaTransportClient::kTransportVolume);

  // Changes volume to a valid level.
  fake_transport_->SetVolume(media_endpoint_->object_path(), 100);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 2);
  EXPECT_EQ(audio_sink_->GetVolume(), 100);

  // Changes volume to an invalid level.
  fake_transport_->SetVolume(media_endpoint_->object_path(), 200);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 3);
  EXPECT_EQ(audio_sink_->GetVolume(), BluetoothAudioSink::kInvalidVolume);
}

TEST_F(BluetoothAudioSinkChromeOSTest, AcquireFD) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  fake_transport_->SetState(media_endpoint_->object_path(), "pending");

  std::vector<char> data_one(16, 0x12);
  fake_transport_->WriteData(media_endpoint_->object_path(), data_one);

  message_loop_.RunUntilIdle();

  // Acquire is called when the state of |audio_sink_| becomes STATE_PENDING,
  // and Acquire will trigger state change. Therefore, the state will be
  // STATE_ACTIVE right after STATE_PENDING.
  // State: STATE_IDLE -> STATE_PENDING -> STATE_ACTIVE
  EXPECT_EQ(observer_->state_changed_count_, 3);
  EXPECT_EQ(observer_->total_read_, data_one.size());
  EXPECT_EQ(observer_->data_, data_one);
}

// Tests the case where the remote device pauses and resume audio streaming.
TEST_F(BluetoothAudioSinkChromeOSTest, PauseAndResume) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  fake_transport_->SetState(media_endpoint_->object_path(), "pending");

  std::vector<char> data_one(16, 0x12);
  fake_transport_->WriteData(media_endpoint_->object_path(), data_one);

  message_loop_.RunUntilIdle();

  EXPECT_EQ(observer_->data_, data_one);
  EXPECT_EQ(observer_->state_changed_count_, 3);
  EXPECT_EQ(observer_->total_read_, data_one.size());

  // Simulates the situation where the remote device pauses and resume audio
  // streaming.
  fake_transport_->SetState(media_endpoint_->object_path(), "idle");

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(observer_->state_changed_count_, 4);

  fake_transport_->SetState(media_endpoint_->object_path(), "pending");

  std::vector<char> data_two(8, 0x10);
  fake_transport_->WriteData(media_endpoint_->object_path(), data_two);

  message_loop_.RunUntilIdle();

  EXPECT_EQ(observer_->data_, data_two);
  EXPECT_EQ(observer_->state_changed_count_, 6);
  EXPECT_EQ(observer_->total_read_, data_two.size());
}

TEST_F(BluetoothAudioSinkChromeOSTest, ContinuouslyStreaming) {
  GetAudioSink();

  media_endpoint_->SelectConfiguration(
      std::vector<uint8_t>({0x21, 0x15, 0x33, 0x2C}),
      base::Bind(&BluetoothAudioSinkChromeOSTest::SelectConfigurationCallback,
                 base::Unretained(this)));

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_DISCONNECTED);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 0);
  EXPECT_EQ(observer_->volume_changed_count_, 0);

  media_endpoint_->SetConfiguration(
      fake_transport_->GetTransportPath(media_endpoint_->object_path()),
      properties_);

  EXPECT_EQ(audio_sink_->GetState(), BluetoothAudioSink::STATE_IDLE);
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(error_callback_count_, 0);
  EXPECT_EQ(observer_->state_changed_count_, 1);
  EXPECT_EQ(observer_->volume_changed_count_, 1);

  fake_transport_->SetState(media_endpoint_->object_path(), "pending");

  std::vector<char> data_one(16, 0x12);
  fake_transport_->WriteData(media_endpoint_->object_path(), data_one);

  message_loop_.RunUntilIdle();

  EXPECT_EQ(observer_->data_, data_one);
  EXPECT_EQ(observer_->state_changed_count_, 3);
  EXPECT_EQ(observer_->total_read_, data_one.size());

  std::vector<char> data_two(8, 0x10);
  fake_transport_->WriteData(media_endpoint_->object_path(), data_two);

  message_loop_.RunUntilIdle();

  EXPECT_EQ(observer_->data_, data_two);
  EXPECT_EQ(observer_->state_changed_count_, 3);
  EXPECT_EQ(observer_->total_read_, data_one.size() + data_two.size());
}

}  // namespace chromeos
