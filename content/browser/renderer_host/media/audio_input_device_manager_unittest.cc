// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_input_device_manager_event_handler.h"
#include "media/audio/audio_manager_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Return;
using content::BrowserThread;

using content::BrowserThreadImpl;

namespace media_stream {

class MockAudioInputDeviceManagerListener
    : public MediaStreamProviderListener {
 public:
  MockAudioInputDeviceManagerListener() {}
  virtual ~MockAudioInputDeviceManagerListener() {}

  MOCK_METHOD2(Opened, void(MediaStreamType, const int));
  MOCK_METHOD2(Closed, void(MediaStreamType, const int));
  MOCK_METHOD1(DevicesEnumerated, void(const StreamDeviceInfoArray&));
  MOCK_METHOD3(Error, void(MediaStreamType, int, MediaStreamProviderError));

  virtual void DevicesEnumerated(MediaStreamType service_type,
                                 const StreamDeviceInfoArray& devices) {
    if (service_type != kAudioCapture)
      return;

    devices_ = devices;
    DevicesEnumerated(devices);
  }

  StreamDeviceInfoArray devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputDeviceManagerListener);
};

class MockAudioInputDeviceManagerEventHandler
    : public AudioInputDeviceManagerEventHandler {
 public:
  MockAudioInputDeviceManagerEventHandler() {}
  virtual ~MockAudioInputDeviceManagerEventHandler() {}

  MOCK_METHOD2(OnDeviceStarted, void(int, const std::string&));
  MOCK_METHOD1(OnDeviceStopped, void(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputDeviceManagerEventHandler);
};

// Returns true if machine has audio input device, else returns false.
static bool CanRunAudioInputDeviceTests() {
  AudioManager* audio_manager = AudioManager::GetAudioManager();
  if (!audio_manager)
    return false;

  return audio_manager->HasAudioInputDevices();
}

ACTION_P(ExitMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

class AudioInputDeviceManagerTest: public testing::Test {
 public:
  AudioInputDeviceManagerTest()
      : message_loop_(),
        io_thread_(),
        manager_(),
        audio_input_listener_() {}

 protected:
  virtual void SetUp() {
    // The test must run on Browser::IO.
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    manager_.reset(new media_stream::AudioInputDeviceManager());
    audio_input_listener_.reset(new MockAudioInputDeviceManagerListener());
    manager_->Register(audio_input_listener_.get());

    // Get the enumerated device list from the AudioInputDeviceManager.
    manager_->EnumerateDevices();
    EXPECT_CALL(*audio_input_listener_, DevicesEnumerated(_))
        .Times(1);
    // Sync up the threads to make sure we get the list.
    SyncWithAudioInputDeviceManagerThread();
  }

  virtual void TearDown() {
    manager_->Unregister();
    io_thread_.reset();
  }

  // Called on the AudioInputDeviceManager thread.
  static void PostQuitMessageLoop(MessageLoop* message_loop) {
    message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  // Called on the main thread.
  static void PostQuitOnAudioInputDeviceManagerThread(
      MessageLoop* message_loop, AudioInputDeviceManager* manager) {
    manager->message_loop()->PostTask(
        FROM_HERE, base::Bind(&PostQuitMessageLoop, message_loop));
  }

  // SyncWithAudioInputDeviceManagerThread() waits until all pending tasks on
  // the audio_input_device_manager thread are executed while also processing
  // pending task in message_loop_ on the current thread.
  void SyncWithAudioInputDeviceManagerThread() {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&PostQuitOnAudioInputDeviceManagerThread,
                   message_loop_.get(),
                   manager_.get()));
    message_loop_->Run();
  }
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<AudioInputDeviceManager> manager_;
  scoped_ptr<MockAudioInputDeviceManagerListener> audio_input_listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManagerTest);
};

// Test the devices can be opened and closed.
TEST_F(AudioInputDeviceManagerTest, OpenAndCloseDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  for (StreamDeviceInfoArray::const_iterator iter =
       audio_input_listener_->devices_.begin();
       iter != audio_input_listener_->devices_.end(); ++iter) {
    // Open/close the devices.
    int session_id = manager_->Open(*iter);
    manager_->Close(session_id);

    // Expected mock call with expected return value.
    EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, session_id))
        .Times(1);
    EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, session_id))
        .Times(1);
    SyncWithAudioInputDeviceManagerThread();
  }
}

// Open multiple devices at one time and close them later.
TEST_F(AudioInputDeviceManagerTest, OpenMultipleDevices) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  int index = 0;
  const int kDeviceSize = audio_input_listener_->devices_.size();
  scoped_array<int> session_id(new int[kDeviceSize]);

  // Open the devices in a loop.
  for (StreamDeviceInfoArray::const_iterator iter =
       audio_input_listener_->devices_.begin();
       iter != audio_input_listener_->devices_.end(); ++iter, ++index) {
    // Open the devices.
    session_id[index] = manager_->Open(*iter);

    // Expected mock call with expected return value.
    EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    SyncWithAudioInputDeviceManagerThread();
  }

  // Check if the session_ids are unique
  for (int i = 0; i < kDeviceSize - 1; ++i) {
    for (int k = i+1; k < kDeviceSize; ++k) {
      EXPECT_TRUE(session_id[i] != session_id[k]);
    }
  }

  for (int i = 0; i < kDeviceSize; ++i) {
    // Close the devices.
    manager_->Close(session_id[i]);
    EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, session_id[i]))
        .Times(1);
    SyncWithAudioInputDeviceManagerThread();
  }
}

// Try to open a non-existing device.
TEST_F(AudioInputDeviceManagerTest, OpenNotExistingDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  MediaStreamType stream_type = kAudioCapture;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  StreamDeviceInfo dummy_device(stream_type, device_name, device_id, false);

  // This should fail and trigger error code 'kDeviceNotAvailable'.
  int session_id = manager_->Open(dummy_device);

  EXPECT_CALL(*audio_input_listener_, Error(_, session_id, kDeviceNotAvailable))
      .Times(1);
  SyncWithAudioInputDeviceManagerThread();
}

// Try open an invalid device.
TEST_F(AudioInputDeviceManagerTest, OpenInvalidDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  MediaStreamType stream_type = kAudioCapture;
  std::string device_name;
  std::string device_id;
  device_name = audio_input_listener_->devices_.front().name;
  device_id = "wrong_id";
  StreamDeviceInfo invalid_device(stream_type, device_name, device_id, false);

  // This should fail and trigger error code 'kDeviceNotAvailable'.
  int session_id = manager_->Open(invalid_device);

  EXPECT_CALL(*audio_input_listener_, Error(_, session_id, kDeviceNotAvailable))
      .Times(1);
  SyncWithAudioInputDeviceManagerThread();
}

// Opening default device twice should work.
TEST_F(AudioInputDeviceManagerTest, OpenDeviceTwice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  // Open/close the default device twice.
  int first_session_id = manager_->Open(
      audio_input_listener_->devices_.front());
  int second_session_id = manager_->Open(
      audio_input_listener_->devices_.front());
  manager_->Close(first_session_id);
  manager_->Close(second_session_id);

  // Expected mock calls with expected return values.
  EXPECT_NE(first_session_id, second_session_id);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, second_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, second_session_id))
      .Times(1);
  SyncWithAudioInputDeviceManagerThread();
}

// Test the Start and Close function after opening the devices.
TEST_F(AudioInputDeviceManagerTest, StartAndStopDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  int index = 0;
  const int kDeviceSize = audio_input_listener_->devices_.size();
  scoped_array<int> session_id(new int[kDeviceSize]);

  // Create the EventHandler for the sessions.
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      audio_input_event_handler(new MockAudioInputDeviceManagerEventHandler());

  // Loop through the devices, and Open/start/stop/close each device.
  for (StreamDeviceInfoArray::const_iterator iter =
       audio_input_listener_->devices_.begin();
       iter != audio_input_listener_->devices_.end(); ++iter, ++index) {
    // Note that no stop device notification for Event Handler as we have
    // stopped the device before calling close.
    // Open/start/stop/close the device.
    session_id[index] = manager_->Open(*iter);
    manager_->Start(session_id[index], audio_input_event_handler.get());
    manager_->Stop(session_id[index]);
    manager_->Close(session_id[index]);

    // Expected mock calls with expected return values.
    EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    EXPECT_CALL(*audio_input_event_handler,
                OnDeviceStarted(session_id[index], iter->device_id))
        .Times(1);
    EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    SyncWithAudioInputDeviceManagerThread();
  }
}

// Test the behavior of calling Close without calling Stop.
TEST_F(AudioInputDeviceManagerTest, CloseWithoutStopDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  int index = 0;
  const int kDeviceSize = audio_input_listener_->devices_.size();
  scoped_array<int> session_id(new int[kDeviceSize]);

  // Create the EventHandlers for the sessions.
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      audio_input_event_handler(new MockAudioInputDeviceManagerEventHandler());

  // Loop through the devices, and open/start/close the devices.
  // Note that we do not call stop.
  for (StreamDeviceInfoArray::const_iterator iter =
       audio_input_listener_->devices_.begin();
       iter != audio_input_listener_->devices_.end(); ++iter, ++index) {
    // Open/start/close the device.
    session_id[index] = manager_->Open(*iter);
    manager_->Start(session_id[index], audio_input_event_handler.get());
    manager_->Close(session_id[index]);

    // Expected mock calls with expected return values.
    EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    EXPECT_CALL(*audio_input_event_handler,
                OnDeviceStarted(session_id[index], iter->device_id))
        .Times(1);
    // Event Handler should get a stop device notification as no stop is called
    // before closing the device.
    EXPECT_CALL(*audio_input_event_handler,
                OnDeviceStopped(session_id[index]))
        .Times(1);
    EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    SyncWithAudioInputDeviceManagerThread();
  }
}

// Should be able to start the default device twice.
TEST_F(AudioInputDeviceManagerTest, StartDeviceTwice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  // Create one EventHandler for each session.
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      first_audio_input_event_handler(
          new MockAudioInputDeviceManagerEventHandler());
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      second_audio_input_event_handler(
          new MockAudioInputDeviceManagerEventHandler());

  // Open the default device twice.
  StreamDeviceInfoArray::const_iterator iter =
      audio_input_listener_->devices_.begin();
  int first_session_id = manager_->Open(*iter);
  int second_session_id = manager_->Open(*iter);

  // Start/stop/close the default device twice.
  manager_->Start(first_session_id, first_audio_input_event_handler.get());
  manager_->Start(second_session_id, second_audio_input_event_handler.get());
  manager_->Stop(first_session_id);
  manager_->Stop(second_session_id);
  manager_->Close(first_session_id);
  manager_->Close(second_session_id);

  // Expected mock calls with expected return values.
  EXPECT_NE(first_session_id, second_session_id);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, second_session_id))
      .Times(1);
  EXPECT_CALL(*first_audio_input_event_handler,
              OnDeviceStarted(first_session_id,
                              AudioManagerBase::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*second_audio_input_event_handler,
              OnDeviceStarted(second_session_id,
                              AudioManagerBase::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, second_session_id))
      .Times(1);
  SyncWithAudioInputDeviceManagerThread();
}

}  // namespace media_stream
