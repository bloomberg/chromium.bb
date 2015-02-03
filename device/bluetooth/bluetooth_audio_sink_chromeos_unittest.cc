// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_audio_sink.h"
#include "device/bluetooth/bluetooth_audio_sink_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    chromeos::DBusThreadManager::Initialize();

    callback_count_ = 0;
    error_callback_count_ = 0;
    audio_sink_ = nullptr;
    adapter_ = nullptr;

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

  // Get the existing Bluetooth adapter.
  void GetAdapter() {
    BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothAudioSinkChromeOSTest::GetAdapterCallback,
                   base::Unretained(this)));
  }

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
    ASSERT_EQ(callback_count_, 1);
    ASSERT_EQ(error_callback_count_, 0);
    --callback_count_;
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
    ASSERT_EQ(error_code, BluetoothAudioSink::ERROR_NOT_REGISTERED);
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
};

TEST_F(BluetoothAudioSinkChromeOSTest, RegisterSucceeded) {
  // Sets up valid codec and capabilities.
  BluetoothAudioSink::Options options;
  ASSERT_EQ(options.codec, 0x00);
  ASSERT_EQ(options.capabilities,
            std::vector<uint8_t>({0x3f, 0xff, 0x12, 0x35}));
  adapter_->RegisterAudioSink(
      options,
      base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterErrorCallback,
                 base::Unretained(this)));

  // Adds observer for the audio sink.
  TestAudioSinkObserver observer(audio_sink_);

  ASSERT_EQ(callback_count_, 1);
  ASSERT_EQ(error_callback_count_, 0);
  ASSERT_EQ(observer.state_changed_count_, 0);
  ASSERT_EQ(observer.volume_changed_count_, 0);
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

  ASSERT_EQ(callback_count_, 0);
  ASSERT_EQ(error_callback_count_, 1);

  // Sets options with a valid codec and invalid capabilities.
  options.codec = 0x00;
  options.capabilities.clear();
  adapter_->RegisterAudioSink(
      options,
      base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAudioSinkChromeOSTest::RegisterErrorCallback,
                 base::Unretained(this)));

  ASSERT_EQ(callback_count_, 0);
  ASSERT_EQ(error_callback_count_, 2);
}

}  // namespace chromeos
