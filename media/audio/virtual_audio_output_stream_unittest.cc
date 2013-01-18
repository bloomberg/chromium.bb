// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "media/audio/virtual_audio_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media {

class MockVirtualAudioInputStream : public VirtualAudioInputStream {
 public:
  MockVirtualAudioInputStream(const AudioParameters& params,
                              base::MessageLoopProxy* message_loop,
                              const AfterCloseCallback& after_close_cb)
      : VirtualAudioInputStream(params, message_loop, after_close_cb) {}
  ~MockVirtualAudioInputStream() {}

  MOCK_METHOD2(AddOutputStream, void(VirtualAudioOutputStream* stream,
                                     const AudioParameters& output_params));
  MOCK_METHOD2(RemoveOutputStream, void(VirtualAudioOutputStream* stream,
                                        const AudioParameters& output_params));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVirtualAudioInputStream);
};

class MockAudioDeviceListener : public AudioManager::AudioDeviceListener {
 public:
  MOCK_METHOD0(OnDeviceChange, void());
};

class VirtualAudioOutputStreamTest : public testing::Test {
 public:
  VirtualAudioOutputStreamTest()
      : audio_manager_(AudioManager::Create()) {}

  scoped_refptr<base::MessageLoopProxy> audio_message_loop() const {
    return audio_manager_->GetMessageLoop();
  }

  void ListenAndCreateVirtualOnAudioThread(
      AudioManager::AudioDeviceListener* listener) {
    audio_manager_->AddOutputDeviceChangeListener(listener);

    AudioParameters params(
        AudioParameters::AUDIO_VIRTUAL, CHANNEL_LAYOUT_MONO, 8000, 8, 128);
    AudioInputStream* stream =
        audio_manager_->MakeAudioInputStream(params, "1");
    stream->Open();
    stream->Close();  // AudioManager deletes |stream|.
  }

  void RemoveListenerOnAudioThread(
      AudioManager::AudioDeviceListener* listener) {
    audio_manager_->RemoveOutputDeviceChangeListener(listener);
  }

  void SyncWithAudioThread() {
    base::WaitableEvent done(false, false);
    audio_message_loop()->PostTask(
        FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                              base::Unretained(&done)));
    done.Wait();
  }

 private:
  scoped_ptr<AudioManager> audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioOutputStreamTest);
};

TEST_F(VirtualAudioOutputStreamTest, StartStopStartStop) {
  static const int kCycles = 3;

  AudioParameters params(
      AudioParameters::AUDIO_VIRTUAL, CHANNEL_LAYOUT_MONO, 8000, 8, 128);
  AudioParameters output_params(
      AudioParameters::AUDIO_FAKE, CHANNEL_LAYOUT_MONO, 8000, 8, 128);

  MockVirtualAudioInputStream* const input_stream =
      new MockVirtualAudioInputStream(
          params, audio_message_loop(),
          base::Bind(&base::DeletePointer<VirtualAudioInputStream>));
  audio_message_loop()->PostTask(
      FROM_HERE, base::Bind(
          base::IgnoreResult(&MockVirtualAudioInputStream::Open),
          base::Unretained(input_stream)));

  VirtualAudioOutputStream* const output_stream =
      new VirtualAudioOutputStream(
          output_params, audio_message_loop(), input_stream,
          base::Bind(&base::DeletePointer<VirtualAudioOutputStream>));

  EXPECT_CALL(*input_stream, AddOutputStream(output_stream, _))
      .Times(kCycles);
  EXPECT_CALL(*input_stream, RemoveOutputStream(output_stream, _))
      .Times(kCycles);

  audio_message_loop()->PostTask(
      FROM_HERE, base::Bind(base::IgnoreResult(&VirtualAudioOutputStream::Open),
                            base::Unretained(output_stream)));
  SineWaveAudioSource source(CHANNEL_LAYOUT_STEREO, 200.0, 128);
  for (int i = 0; i < kCycles; ++i) {
    audio_message_loop()->PostTask(
        FROM_HERE, base::Bind(&VirtualAudioOutputStream::Start,
                              base::Unretained(output_stream),
                              &source));
    audio_message_loop()->PostTask(
        FROM_HERE, base::Bind(&VirtualAudioOutputStream::Stop,
                              base::Unretained(output_stream)));
  }
  audio_message_loop()->PostTask(
      FROM_HERE, base::Bind(&VirtualAudioOutputStream::Close,
                            base::Unretained(output_stream)));

  audio_message_loop()->PostTask(
      FROM_HERE, base::Bind(&MockVirtualAudioInputStream::Close,
                            base::Unretained(input_stream)));

  SyncWithAudioThread();
}

// Tests that we get notifications to reattach output streams when we create a
// VirtualAudioInputStream.
TEST_F(VirtualAudioOutputStreamTest, OutputStreamsNotified) {
  MockAudioDeviceListener mock_listener;
  EXPECT_CALL(mock_listener, OnDeviceChange()).Times(2);

  audio_message_loop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioOutputStreamTest::ListenAndCreateVirtualOnAudioThread,
          base::Unretained(this),
          &mock_listener));

  SyncWithAudioThread();

  audio_message_loop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioOutputStreamTest::RemoveListenerOnAudioThread,
          base::Unretained(this),
          &mock_listener));

  SyncWithAudioThread();
}

}  // namespace media
