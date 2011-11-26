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
  explicit MockAudioInputDeviceManagerEventHandler(MessageLoop* message_loop)
      : message_loop_(message_loop) {}
  virtual ~MockAudioInputDeviceManagerEventHandler() {}

  MOCK_METHOD2(DeviceStarted, void(int, const std::string&));
  MOCK_METHOD1(DeviceStopped, void(int));

  virtual void OnDeviceStarted(int session_id,
                               const std::string& device_id) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(
            &MockAudioInputDeviceManagerEventHandler::DeviceStarted,
            base::Unretained(this), session_id, device_id));
  }

  virtual void OnDeviceStopped(int session_id) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(
            &MockAudioInputDeviceManagerEventHandler::DeviceStopped,
            base::Unretained(this), session_id));
  }

 private:
  MessageLoop* message_loop_;
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

    // Gets the enumerated device list from the AudioInputDeviceManager.
    manager_->EnumerateDevices();
    EXPECT_CALL(*audio_input_listener_, DevicesEnumerated(_))
        .Times(1);

    // Waits for the callback.
    message_loop_->RunAllPending();
  }

  virtual void TearDown() {
    manager_->Unregister();
    io_thread_.reset();
  }

  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<AudioInputDeviceManager> manager_;
  scoped_ptr<MockAudioInputDeviceManagerListener> audio_input_listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManagerTest);
};

// Opens and closes the devices.
TEST_F(AudioInputDeviceManagerTest, OpenAndCloseDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  for (StreamDeviceInfoArray::const_iterator iter =
       audio_input_listener_->devices_.begin();
       iter != audio_input_listener_->devices_.end(); ++iter) {
    // Opens/closes the devices.
    int session_id = manager_->Open(*iter);
    manager_->Close(session_id);

    // Expected mock call with expected return value.
    EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, session_id))
        .Times(1);
    EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, session_id))
        .Times(1);

    // Waits for the callback.
    message_loop_->RunAllPending();
  }
}

// Opens multiple devices at one time and closes them later.
TEST_F(AudioInputDeviceManagerTest, OpenMultipleDevices) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  int index = 0;
  const int kDeviceSize = audio_input_listener_->devices_.size();
  scoped_array<int> session_id(new int[kDeviceSize]);

  // Opens the devices in a loop.
  for (StreamDeviceInfoArray::const_iterator iter =
       audio_input_listener_->devices_.begin();
       iter != audio_input_listener_->devices_.end(); ++iter, ++index) {
    // Opens the devices.
    session_id[index] = manager_->Open(*iter);

    // Expected mock call with expected returned value.
    EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture,
                                               session_id[index]))
        .Times(1);

    // Waits for the callback.
    message_loop_->RunAllPending();
  }

  // Checks if the session_ids are unique.
  for (int i = 0; i < kDeviceSize - 1; ++i) {
    for (int k = i+1; k < kDeviceSize; ++k) {
      EXPECT_TRUE(session_id[i] != session_id[k]);
    }
  }

  for (int i = 0; i < kDeviceSize; ++i) {
    // Closes the devices.
    manager_->Close(session_id[i]);
    EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, session_id[i]))
        .Times(1);

    // Waits for the callback.
    message_loop_->RunAllPending();
  }
}

// Opens a non-existing device.
TEST_F(AudioInputDeviceManagerTest, OpenNotExistingDevice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  MediaStreamType stream_type = kAudioCapture;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  StreamDeviceInfo dummy_device(stream_type, device_name, device_id, false);

  int session_id = manager_->Open(dummy_device);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, session_id))
      .Times(1);

  // Waits for the callback.
  message_loop_->RunAllPending();
}

// Opens default device twice.
TEST_F(AudioInputDeviceManagerTest, OpenDeviceTwice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  // Opens and closes the default device twice.
  int first_session_id = manager_->Open(
      audio_input_listener_->devices_.front());
  int second_session_id = manager_->Open(
      audio_input_listener_->devices_.front());
  manager_->Close(first_session_id);
  manager_->Close(second_session_id);

  // Expected mock calls with expected returned values.
  EXPECT_NE(first_session_id, second_session_id);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, second_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, second_session_id))
      .Times(1);

  // Waits for the callback.
  message_loop_->RunAllPending();
}

// Starts and closes the sessions after opening the devices.
TEST_F(AudioInputDeviceManagerTest, StartAndStopSession) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  int index = 0;
  const int kDeviceSize = audio_input_listener_->devices_.size();
  scoped_array<int> session_id(new int[kDeviceSize]);

  // Creates the EventHandler for the sessions.
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      audio_input_event_handler(
          new MockAudioInputDeviceManagerEventHandler(message_loop_.get()));

  // Loops through the devices and calls Open()/Start()/Stop()/Close() for
  // each device.
  for (StreamDeviceInfoArray::const_iterator iter =
       audio_input_listener_->devices_.begin();
       iter != audio_input_listener_->devices_.end(); ++iter, ++index) {
    // Note that no DeviceStopped() notification for Event Handler as we have
    // stopped the device before calling close.
    session_id[index] = manager_->Open(*iter);
    EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    message_loop_->RunAllPending();

    manager_->Start(session_id[index], audio_input_event_handler.get());
    EXPECT_CALL(*audio_input_event_handler,
                DeviceStarted(session_id[index], iter->device_id))
        .Times(1);
    message_loop_->RunAllPending();

    manager_->Stop(session_id[index]);
    manager_->Close(session_id[index]);
    EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    message_loop_->RunAllPending();
  }
}

// Tests the behavior of calling Close() without calling Stop().
TEST_F(AudioInputDeviceManagerTest, CloseWithoutStopSession) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  int index = 0;
  const int kDeviceSize = audio_input_listener_->devices_.size();
  scoped_array<int> session_id(new int[kDeviceSize]);

  // Creates the EventHandlers for the sessions.
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      audio_input_event_handler(
          new MockAudioInputDeviceManagerEventHandler(message_loop_.get()));

  // Loop through the devices, and calls Open()/Start()/Close() for the devices.
  // Note that we do not call stop.
  for (StreamDeviceInfoArray::const_iterator iter =
       audio_input_listener_->devices_.begin();
       iter != audio_input_listener_->devices_.end(); ++iter, ++index) {
    // Calls Open()/Start()/Close() for each device.
    session_id[index] = manager_->Open(*iter);
    EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    message_loop_->RunAllPending();

    manager_->Start(session_id[index], audio_input_event_handler.get());
    EXPECT_CALL(*audio_input_event_handler,
                DeviceStarted(session_id[index], iter->device_id))
        .Times(1);
    message_loop_->RunAllPending();

    // Event Handler should get a stop device notification as no stop is called
    // before closing the device.
    manager_->Close(session_id[index]);
    EXPECT_CALL(*audio_input_event_handler,
                DeviceStopped(session_id[index]))
        .Times(1);
    EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture,
                                               session_id[index]))
        .Times(1);
    message_loop_->RunAllPending();
  }
}

// Starts the same device twice.
TEST_F(AudioInputDeviceManagerTest, StartDeviceTwice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  // Create one EventHandler for each session.
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      first_event_handler(
          new MockAudioInputDeviceManagerEventHandler(message_loop_.get()));
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      second_event_handler(
          new MockAudioInputDeviceManagerEventHandler(message_loop_.get()));

  // Open the default device twice.
  StreamDeviceInfoArray::const_iterator iter =
      audio_input_listener_->devices_.begin();
  int first_session_id = manager_->Open(*iter);
  int second_session_id = manager_->Open(*iter);
  EXPECT_NE(first_session_id, second_session_id);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, second_session_id))
      .Times(1);
  message_loop_->RunAllPending();

  // Calls Start()/Stop()/Close() for the default device twice.
  manager_->Start(first_session_id, first_event_handler.get());
  manager_->Start(second_session_id, second_event_handler.get());
  EXPECT_CALL(*first_event_handler,
              DeviceStarted(first_session_id,
                            AudioManagerBase::kDefaultDeviceId))
      .Times(1);
  EXPECT_CALL(*second_event_handler,
              DeviceStarted(second_session_id,
                            AudioManagerBase::kDefaultDeviceId))
      .Times(1);
  message_loop_->RunAllPending();

  manager_->Stop(first_session_id);
  manager_->Stop(second_session_id);
  manager_->Close(first_session_id);
  manager_->Close(second_session_id);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, second_session_id))
      .Times(1);
  message_loop_->RunAllPending();
}

// Starts an invalid session.
TEST_F(AudioInputDeviceManagerTest, StartInvalidSession) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  // Creates the EventHandlers for the sessions.
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      audio_input_event_handler(
          new MockAudioInputDeviceManagerEventHandler(message_loop_.get()));

  // Opens the first device.
  StreamDeviceInfoArray::const_iterator iter =
      audio_input_listener_->devices_.begin();
  int session_id = manager_->Open(*iter);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, session_id))
      .Times(1);
  message_loop_->RunAllPending();

  // Starts a non-opened device.
  // This should fail and trigger error code 'kDeviceNotAvailable'.
  int invalid_session_id = session_id + 1;
  manager_->Start(invalid_session_id, audio_input_event_handler.get());
  EXPECT_CALL(*audio_input_event_handler,
              DeviceStarted(invalid_session_id,
                            AudioInputDeviceManager::kInvalidDeviceId))
      .Times(1);
  message_loop_->RunAllPending();

  manager_->Close(session_id);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, session_id))
      .Times(1);
  message_loop_->RunAllPending();
}

// Starts a session twice, the first time should succeed, while the second
// time should fail.
TEST_F(AudioInputDeviceManagerTest, StartSessionTwice) {
  if (!CanRunAudioInputDeviceTests())
    return;
  InSequence s;

  // Creates the EventHandlers for the sessions.
  scoped_ptr<MockAudioInputDeviceManagerEventHandler>
      audio_input_event_handler(
          new MockAudioInputDeviceManagerEventHandler(message_loop_.get()));

  // Opens the first device.
  StreamDeviceInfoArray::const_iterator iter =
      audio_input_listener_->devices_.begin();
  int session_id = manager_->Open(*iter);
  EXPECT_CALL(*audio_input_listener_, Opened(kAudioCapture, session_id))
      .Times(1);
  message_loop_->RunAllPending();

  // Starts the session, it should succeed.
  manager_->Start(session_id, audio_input_event_handler.get());
  EXPECT_CALL(*audio_input_event_handler,
              DeviceStarted(session_id,
                            AudioManagerBase::kDefaultDeviceId))
      .Times(1);
  message_loop_->RunAllPending();

  // Starts the session for the second time, it should fail.
  manager_->Start(session_id, audio_input_event_handler.get());
  EXPECT_CALL(*audio_input_event_handler,
              DeviceStarted(session_id,
                            AudioInputDeviceManager::kInvalidDeviceId))
      .Times(1);

  manager_->Stop(session_id);
  manager_->Close(session_id);
  EXPECT_CALL(*audio_input_listener_, Closed(kAudioCapture, session_id))
      .Times(1);
  message_loop_->RunAllPending();
}

}  // namespace media_stream
