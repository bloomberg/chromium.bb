// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;

namespace media {

namespace {

class MockInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MockInputCallback()
      : data_pushed_(false, false) {
    ON_CALL(*this, OnData(_, _, _, _, _))
        .WillByDefault(InvokeWithoutArgs(&data_pushed_,
                                         &base::WaitableEvent::Signal));
  }

  virtual ~MockInputCallback() {}

  MOCK_METHOD5(OnData, void(AudioInputStream* stream, const uint8* data,
                            uint32 size, uint32 hardware_delay_bytes,
                            double volume));
  MOCK_METHOD1(OnClose, void(AudioInputStream* stream));
  MOCK_METHOD2(OnError, void(AudioInputStream* stream, int code));

  void WaitForDataPushes() {
    for (int i = 0; i < 3; ++i) {
      data_pushed_.Wait();
    }
  }

 private:
  base::WaitableEvent data_pushed_;

  DISALLOW_COPY_AND_ASSIGN(MockInputCallback);
};

class TestAudioSource : public SineWaveAudioSource {
 public:
  TestAudioSource()
      : SineWaveAudioSource(CHANNEL_LAYOUT_STEREO, 200.0, 8000.0),
        data_pulled_(false, false) {}

  virtual ~TestAudioSource() {}

  virtual int OnMoreData(AudioBus* audio_bus,
                         AudioBuffersState audio_buffers) OVERRIDE {
    const int ret = SineWaveAudioSource::OnMoreData(audio_bus, audio_buffers);
    data_pulled_.Signal();
    return ret;
  }

  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState audio_buffers) OVERRIDE {
    const int ret =
        SineWaveAudioSource::OnMoreIOData(source, dest, audio_buffers);
    data_pulled_.Signal();
    return ret;
  }

  void WaitForDataPulls() {
    for (int i = 0; i < 3; ++i) {
      data_pulled_.Wait();
    }
  }

 private:
  base::WaitableEvent data_pulled_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioSource);
};

}  // namespace

class VirtualAudioInputStreamTest
    : public testing::Test,
      public AudioManager::AudioDeviceListener {
 public:
  VirtualAudioInputStreamTest()
      : audio_manager_(AudioManager::Create()),
        params_(AudioParameters::AUDIO_VIRTUAL, CHANNEL_LAYOUT_STEREO, 8000, 8,
                10),
        output_params_(AudioParameters::AUDIO_FAKE, CHANNEL_LAYOUT_STEREO, 8000,
                       8, 10),
        stream_(NULL),
        closed_stream_(false, false) {
    audio_manager_->GetMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&AudioManager::AddOutputDeviceChangeListener,
                   base::Unretained(audio_manager_.get()),
                   this));
  }

  ~VirtualAudioInputStreamTest() {
    audio_manager_->GetMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&AudioManager::RemoveOutputDeviceChangeListener,
                   base::Unretained(audio_manager_.get()),
                   this));

    SyncWithAudioThread();

    DCHECK(output_streams_.empty());
    DCHECK(stopped_output_streams_.empty());
  }

  void Create() {
    stream_ = static_cast<VirtualAudioInputStream*>(
        audio_manager_->MakeAudioInputStream(params_, "1"));
    ASSERT_TRUE(!!stream_);
    stream_->Open();
  }

  void Start() {
    EXPECT_CALL(input_callback_, OnClose(_));
    EXPECT_CALL(input_callback_, OnData(_, NotNull(), _, _, _))
        .Times(AtLeast(1));

    ASSERT_TRUE(!!stream_);
    stream_->Start(&input_callback_);
  }

  void CreateAndStartOneOutputStream() {
    AudioOutputStream* const output_stream =
        audio_manager_->MakeAudioOutputStreamProxy(output_params_);
    ASSERT_TRUE(!!output_stream);
    output_streams_.push_back(output_stream);

    output_stream->Open();
    output_stream->Start(&source_);
  }

  void Stop() {
    ASSERT_TRUE(!!stream_);
    stream_->Stop();
  }

  void Close() {
    ASSERT_TRUE(!!stream_);
    stream_->Close();
    stream_ = NULL;
    closed_stream_.Signal();
  }

  void WaitForDataPulls() {
    const int count = output_streams_.size();
    for (int i = 0; i < count; ++i) {
      source_.WaitForDataPulls();
    }
  }

  void WaitForDataPushes() {
    input_callback_.WaitForDataPushes();
  }

  void WaitUntilClosed() {
    closed_stream_.Wait();
  }

  void StopAndCloseOneOutputStream() {
    ASSERT_TRUE(!output_streams_.empty());
    AudioOutputStream* const output_stream = output_streams_.front();
    ASSERT_TRUE(!!output_stream);
    output_streams_.pop_front();

    output_stream->Stop();
    output_stream->Close();
  }

  void StopFirstOutputStream() {
    ASSERT_TRUE(!output_streams_.empty());
    AudioOutputStream* const output_stream = output_streams_.front();
    ASSERT_TRUE(!!output_stream);
    output_streams_.pop_front();
    output_stream->Stop();
    stopped_output_streams_.push_back(output_stream);
  }

  void StopSomeOutputStreams() {
    ASSERT_LE(1, static_cast<int>(output_streams_.size()));
    for (int remaning = base::RandInt(1, output_streams_.size() - 1);
         remaning > 0; --remaning) {
      StopFirstOutputStream();
    }
  }

  void RestartAllStoppedOutputStreams() {
    typedef std::list<AudioOutputStream*>::const_iterator ConstIter;
    for (ConstIter it = stopped_output_streams_.begin();
         it != stopped_output_streams_.end(); ++it) {
      (*it)->Start(&source_);
      output_streams_.push_back(*it);
    }
    stopped_output_streams_.clear();
  }

  AudioManager* audio_manager() const { return audio_manager_.get(); }

 private:
  virtual void OnDeviceChange() OVERRIDE {
    audio_manager_->RemoveOutputDeviceChangeListener(this);

    // Simulate each AudioOutputController closing and re-opening a stream, in
    // turn.
    for (int remaining = output_streams_.size(); remaining > 0; --remaining) {
      StopAndCloseOneOutputStream();
      CreateAndStartOneOutputStream();
    }
    for (int remaining = stopped_output_streams_.size(); remaining > 0;
         --remaining) {
      AudioOutputStream* stream = stopped_output_streams_.front();
      stopped_output_streams_.pop_front();
      stream->Close();

      stream = audio_manager_->MakeAudioOutputStreamProxy(output_params_);
      ASSERT_TRUE(!!stream);
      stopped_output_streams_.push_back(stream);
      stream->Open();
    }

    audio_manager_->AddOutputDeviceChangeListener(this);
  }

  void SyncWithAudioThread() {
    base::WaitableEvent done(false, false);
    audio_manager_->GetMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
  }

  const scoped_ptr<AudioManager> audio_manager_;
  const AudioParameters params_;
  const AudioParameters output_params_;

  VirtualAudioInputStream* stream_;
  MockInputCallback input_callback_;
  base::WaitableEvent closed_stream_;

  std::list<AudioOutputStream*> output_streams_;
  std::list<AudioOutputStream*> stopped_output_streams_;
  TestAudioSource source_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioInputStreamTest);
};

#define RUN_ON_AUDIO_THREAD(method)  \
  audio_manager()->GetMessageLoop()->PostTask(  \
      FROM_HERE, base::Bind(&VirtualAudioInputStreamTest::method,  \
                            base::Unretained(this)))

// Creates and closes and VirtualAudioInputStream.
TEST_F(VirtualAudioInputStreamTest, CreateAndClose) {
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

// Tests a session where one output is created and destroyed within.
TEST_F(VirtualAudioInputStreamTest, SingleOutputWithinSession) {
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataPulls();  // Confirm data is being pulled (rendered).
  WaitForDataPushes();  // Confirm mirrored data is being pushed.
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

// Tests a session where one output existed before and afterwards.
TEST_F(VirtualAudioInputStreamTest, SingleLongLivedOutput) {
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataPulls();  // Confirm data is flowing out before the session.
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  WaitForDataPushes();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
  WaitForDataPulls();  // Confirm data is still flowing out.
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
}

TEST_F(VirtualAudioInputStreamTest, SingleOutputPausedWithinSession) {
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataPulls();
  WaitForDataPushes();
  RUN_ON_AUDIO_THREAD(StopFirstOutputStream);
  RUN_ON_AUDIO_THREAD(RestartAllStoppedOutputStreams);
  WaitForDataPulls();
  WaitForDataPushes();
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

TEST_F(VirtualAudioInputStreamTest, OutputStreamsReconnectToRealDevices) {
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataPulls();
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  WaitForDataPushes();
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataPulls();
  WaitForDataPushes();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
  WaitForDataPulls();
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
}

TEST_F(VirtualAudioInputStreamTest, PausedOutputStreamReconnectsToRealDevice) {
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataPulls();
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  WaitForDataPushes();
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataPulls();
  WaitForDataPushes();
  RUN_ON_AUDIO_THREAD(StopFirstOutputStream);
  WaitForDataPulls();
  WaitForDataPushes();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
  WaitForDataPulls();
  RUN_ON_AUDIO_THREAD(RestartAllStoppedOutputStreams);
  WaitForDataPulls();
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
}

// A combination of all of the above tests with many output streams.
TEST_F(VirtualAudioInputStreamTest, ComprehensiveTest) {
  static const int kNumOutputs = 8;
  static const int kPauseIterations = 5;

  for (int i = 0; i < kNumOutputs / 2; ++i) {
    RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  }
  WaitForDataPulls();
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  WaitForDataPushes();
  for (int i = 0; i < kNumOutputs / 2; ++i) {
    RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  }
  WaitForDataPulls();
  WaitForDataPushes();
  for (int i = 0; i < kPauseIterations; ++i) {
    RUN_ON_AUDIO_THREAD(StopSomeOutputStreams);
    WaitForDataPulls();
    WaitForDataPushes();
    RUN_ON_AUDIO_THREAD(RestartAllStoppedOutputStreams);
    WaitForDataPulls();
    WaitForDataPushes();
  }
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
  WaitForDataPulls();
  for (int i = 0; i < kNumOutputs; ++i) {
    RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  }
}

}  // namespace media
