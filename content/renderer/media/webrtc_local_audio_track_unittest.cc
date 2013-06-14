// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

namespace content {

namespace {

ACTION_P(SignalEvent, event) {
  event->Signal();
}

// A simple thread that we use to fake the audio thread which provides data to
// the |WebRtcAudioCapturer|.
class FakeAudioThread : public base::PlatformThread::Delegate {
 public:
  explicit FakeAudioThread(const scoped_refptr<WebRtcAudioCapturer>& capturer)
    : capturer_(capturer),
      thread_(),
      closure_(false, false) {
    DCHECK(capturer.get());
    audio_bus_ = media::AudioBus::Create(capturer_->audio_parameters());
  }

  virtual ~FakeAudioThread() { DCHECK(thread_.is_null()); }

  // base::PlatformThread::Delegate:
  virtual void ThreadMain() OVERRIDE {
    while (true) {
      if (closure_.IsSignaled())
        return;

      media::AudioCapturerSource::CaptureCallback* callback =
          static_cast<media::AudioCapturerSource::CaptureCallback*>(
              capturer_.get());
      audio_bus_->Zero();
      callback->Capture(audio_bus_.get(), 0, 0);

      // Sleep 1ms to yield the resource for the main thread.
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
  }

  void Start() {
    base::PlatformThread::CreateWithPriority(
        0, this, &thread_, base::kThreadPriority_RealtimeAudio);
    CHECK(!thread_.is_null());
  }

  void Stop() {
    closure_.Signal();
    base::PlatformThread::Join(thread_);
    thread_ = base::PlatformThreadHandle();
  }

 private:
  scoped_ptr<media::AudioBus> audio_bus_;
  scoped_refptr<WebRtcAudioCapturer> capturer_;
  base::PlatformThreadHandle thread_;
  base::WaitableEvent closure_;
  DISALLOW_COPY_AND_ASSIGN(FakeAudioThread);
};

class MockCapturerSource : public media::AudioCapturerSource {
 public:
  MockCapturerSource() {}
  MOCK_METHOD3(Initialize, void(const media::AudioParameters& params,
                                CaptureCallback* callback,
                                int session_id));
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetAutomaticGainControl, void(bool enable));

 protected:
  virtual ~MockCapturerSource() {}
};

class MockWebRtcAudioCapturerSink : public WebRtcAudioCapturerSink {
 public:
  MockWebRtcAudioCapturerSink() {}
  ~MockWebRtcAudioCapturerSink() {}
  MOCK_METHOD5(CaptureData, void(const int16* audio_data,
                                 int number_of_channels,
                                 int number_of_frames,
                                 int audio_delay_milliseconds,
                                 double volume));
  MOCK_METHOD1(SetCaptureFormat, void(const media::AudioParameters& params));
};

}  // namespace

class WebRtcLocalAudioTrackTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    capturer_ = WebRtcAudioCapturer::CreateCapturer();
    capturer_source_ = new MockCapturerSource();
    EXPECT_CALL(*capturer_source_.get(), Initialize(_, capturer_.get(), 0))
        .WillOnce(Return());
    capturer_->SetCapturerSource(capturer_source_,
                                 media::CHANNEL_LAYOUT_STEREO,
                                 48000);

    EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(false))
        .WillOnce(Return());
    audio_thread_.reset(new FakeAudioThread(capturer_));
    audio_thread_->Start();
  }

  virtual void TearDown() {
    audio_thread_->Stop();
    audio_thread_.reset();
  }

  scoped_refptr<MockCapturerSource> capturer_source_;
  scoped_refptr<WebRtcAudioCapturer> capturer_;
  scoped_ptr<FakeAudioThread> audio_thread_;
};

// Creates a capturer and audio track, fakes its audio thread, and
// connect/disconnect the sink to the audio track on the fly, the sink should
// get data callback when the track is connected to the capturer but not when
// the track is disconnected from the capturer.
TEST_F(WebRtcLocalAudioTrackTest, ConnectAndDisconnectOneSink) {
  EXPECT_CALL(*capturer_source_.get(), Start()).WillOnce(Return());
  scoped_refptr<WebRtcLocalAudioTrack> track =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL);
  track->Start();
  EXPECT_TRUE(track->enabled());
  scoped_ptr<MockWebRtcAudioCapturerSink> sink(
      new MockWebRtcAudioCapturerSink());
  const media::AudioParameters params = capturer_->audio_parameters();
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*sink, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink, CaptureData(
      _, params.channels(), params.frames_per_buffer(), 0, 0))
      .Times(AtLeast(1)).WillRepeatedly(SignalEvent(&event));
  track->AddSink(sink.get());

  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));
  track->RemoveSink(sink.get());

  EXPECT_CALL(*capturer_source_.get(), Stop()).WillOnce(Return());
  track->Stop();
  track = NULL;
}

// The same setup as ConnectAndDisconnectOneSink, but enable and disable the
// audio track on the fly. When the audio track is disabled, there is no data
// callback to the sink; when the audio track is enabled, there comes data
// callback.
// TODO(xians): Enable this test after resolving the racing issue that TSAN
// reports on MediaStreamTrack::enabled();
TEST_F(WebRtcLocalAudioTrackTest, DISABLED_DisableEnableAudioTrack) {
  EXPECT_CALL(*capturer_source_.get(), Start()).WillOnce(Return());
  scoped_refptr<WebRtcLocalAudioTrack> track =
    WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL);
  track->Start();
  EXPECT_TRUE(track->enabled());
  EXPECT_TRUE(track->set_enabled(false));
  scoped_ptr<MockWebRtcAudioCapturerSink> sink(
      new MockWebRtcAudioCapturerSink());
  const media::AudioParameters params = capturer_->audio_parameters();
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*sink, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink, CaptureData(
      _, params.channels(), params.frames_per_buffer(), 0, 0))
      .Times(0);
  track->AddSink(sink.get());
  EXPECT_FALSE(event.TimedWait(TestTimeouts::tiny_timeout()));

  event.Reset();
  EXPECT_CALL(*sink, CaptureData(
      _, params.channels(), params.frames_per_buffer(), 0, 0))
      .Times(AtLeast(1)).WillRepeatedly(SignalEvent(&event));
  EXPECT_TRUE(track->set_enabled(true));
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));
  track->RemoveSink(sink.get());

  EXPECT_CALL(*capturer_source_.get(), Stop()).WillOnce(Return());
  track->Stop();
  track = NULL;
}

// Create multiple audio tracks and enable/disable them, verify that the audio
// callbacks appear/disappear.
TEST_F(WebRtcLocalAudioTrackTest, MultipleAudioTracks) {
  EXPECT_CALL(*capturer_source_.get(), Start()).WillOnce(Return());
  scoped_refptr<WebRtcLocalAudioTrack> track_1 =
    WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL);
  track_1->Start();
  EXPECT_TRUE(track_1->enabled());
  scoped_ptr<MockWebRtcAudioCapturerSink> sink_1(
      new MockWebRtcAudioCapturerSink());
  const media::AudioParameters params = capturer_->audio_parameters();
  base::WaitableEvent event_1(false, false);
  EXPECT_CALL(*sink_1, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink_1, CaptureData(
      _, params.channels(), params.frames_per_buffer(), 0, 0))
      .Times(AtLeast(1)).WillRepeatedly(SignalEvent(&event_1));
  track_1->AddSink(sink_1.get());
  EXPECT_TRUE(event_1.TimedWait(TestTimeouts::tiny_timeout()));

  scoped_refptr<WebRtcLocalAudioTrack> track_2 =
    WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL);
  track_2->Start();
  EXPECT_TRUE(track_2->enabled());

  // Verify both |sink_1| and |sink_2| get data.
  event_1.Reset();
  base::WaitableEvent event_2(false, false);

  scoped_ptr<MockWebRtcAudioCapturerSink> sink_2(
        new MockWebRtcAudioCapturerSink());
  EXPECT_CALL(*sink_2, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink_1, CaptureData(
      _, params.channels(), params.frames_per_buffer(), 0, 0))
      .Times(AtLeast(1)).WillRepeatedly(SignalEvent(&event_1));
  EXPECT_CALL(*sink_2, CaptureData(
      _, params.channels(), params.frames_per_buffer(), 0, 0))
      .Times(AtLeast(1)).WillRepeatedly(SignalEvent(&event_2));
  track_2->AddSink(sink_2.get());
  EXPECT_TRUE(event_1.TimedWait(TestTimeouts::tiny_timeout()));
  EXPECT_TRUE(event_2.TimedWait(TestTimeouts::tiny_timeout()));

  track_1->RemoveSink(sink_1.get());
  track_1->Stop();
  track_1 = NULL;

  EXPECT_CALL(*capturer_source_.get(), Stop()).WillOnce(Return());
  track_2->RemoveSink(sink_2.get());
  track_2->Stop();
  track_2 = NULL;
}


// Start one track and verify the capturer is correctly starting its source.
// And it should be fine to not to call Stop() explicitly.
TEST_F(WebRtcLocalAudioTrackTest, StartOneAudioTrack) {
  EXPECT_CALL(*capturer_source_.get(), Start()).Times(1);
  scoped_refptr<WebRtcLocalAudioTrack> track =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL);
  track->Start();

  // When the track goes away, it will automatically stop the
  // |capturer_source_|.
  EXPECT_CALL(*capturer_source_.get(), Stop());
  track = NULL;
}

// Start/Stop tracks and verify the capturer is correctly starting/stopping
// its source.
TEST_F(WebRtcLocalAudioTrackTest, StartAndStopAudioTracks) {
  // SetDefaultSink() should not trigger the Start() on |capturer_source_|.
  base::WaitableEvent event(false, false);
  scoped_ptr<MockWebRtcAudioCapturerSink> default_sink(
      new MockWebRtcAudioCapturerSink());
  EXPECT_CALL(*default_sink, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*default_sink, CaptureData(_, _, _, 0, 0))
      .Times(AnyNumber()).WillRepeatedly(Return());
  capturer_->SetDefaultSink(default_sink.get());
  EXPECT_CALL(*capturer_source_.get(), Start()).Times(0);
  EXPECT_FALSE(event.TimedWait(TestTimeouts::tiny_timeout()));

  // Starting the first audio track will start the |capturer_source_|.
  event.Reset();
  EXPECT_CALL(*capturer_source_.get(), Start()).WillOnce(SignalEvent(&event));
  scoped_refptr<WebRtcLocalAudioTrack> track_1 =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL);
  track_1->Start();
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));

  // Start the second audio track will not start the |capturer_source_|
  // since it has been started.
  EXPECT_CALL(*capturer_source_.get(), Start()).Times(0);
  scoped_refptr<WebRtcLocalAudioTrack> track_2 =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL);
  track_2->Start();

  // Stop the first audio track will not stop the |capturer_source_|.
  EXPECT_CALL(*capturer_source_.get(), Stop()).Times(0);
  track_1->Stop();
  track_1 = NULL;

  // Stop the last audio track will stop the |capturer_source_|.
  event.Reset();
  EXPECT_CALL(*capturer_source_.get(), Stop())
      .Times(1).WillOnce(SignalEvent(&event));
  track_2->Stop();
  track_2 = NULL;
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));

  capturer_->SetDefaultSink(NULL);
}

// Set new source to the capturer.
TEST_F(WebRtcLocalAudioTrackTest, SetNewSourceForCapturerAfterStartTrack) {
  // Setup the audio track and start the track.
  EXPECT_CALL(*capturer_source_.get(), Start()).Times(1);
  scoped_refptr<WebRtcLocalAudioTrack> track =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL);
  track->Start();

  // Setting new source to the capturer and the track should still get packets.
  scoped_refptr<MockCapturerSource> new_source(new MockCapturerSource());
  EXPECT_CALL(*capturer_source_.get(), Stop());
  EXPECT_CALL(*new_source.get(), SetAutomaticGainControl(false));
  EXPECT_CALL(*new_source.get(), Initialize(_, capturer_.get(), 0))
      .WillOnce(Return());
  EXPECT_CALL(*new_source.get(), Start()).WillOnce(Return());
  capturer_->SetCapturerSource(new_source,
                               media::CHANNEL_LAYOUT_STEREO,
                               48000);

  // Stop the track.
  EXPECT_CALL(*new_source.get(), Stop());
  track->Stop();
  track = NULL;
}

}  // namespace content
