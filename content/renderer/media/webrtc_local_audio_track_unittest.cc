// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

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
      callback->Capture(audio_bus_.get(), 0, 0, false);

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
  int CaptureData(const std::vector<int>& channels,
                  const int16* audio_data,
                  int sample_rate,
                  int number_of_channels,
                  int number_of_frames,
                  int audio_delay_milliseconds,
                  int current_volume,
                  bool need_audio_processing,
                  bool key_pressed) OVERRIDE {
    CaptureData(channels.size(),
                sample_rate,
                number_of_channels,
                number_of_frames,
                audio_delay_milliseconds,
                current_volume,
                need_audio_processing,
                key_pressed);
    return 0;
  }
  MOCK_METHOD8(CaptureData,
               void(int number_of_network_channels,
                    int sample_rate,
                    int number_of_channels,
                    int number_of_frames,
                    int audio_delay_milliseconds,
                    int current_volume,
                    bool need_audio_processing,
                    bool key_pressed));
  MOCK_METHOD1(SetCaptureFormat, void(const media::AudioParameters& params));
};

}  // namespace

class WebRtcLocalAudioTrackTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                  media::CHANNEL_LAYOUT_STEREO, 2, 0, 48000, 16, 480);
    capturer_ = WebRtcAudioCapturer::CreateCapturer();
    capturer_source_ = new MockCapturerSource();
    EXPECT_CALL(*capturer_source_.get(), Initialize(_, capturer_.get(), 0))
        .WillOnce(Return());
    capturer_->SetCapturerSource(capturer_source_,
                                 params_.channel_layout(),
                                 params_.sample_rate());

    EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true))
        .WillOnce(Return());

    // Start the audio thread used by the |capturer_source_|.
    audio_thread_.reset(new FakeAudioThread(capturer_));
    audio_thread_->Start();
  }

  virtual void TearDown() {
    audio_thread_->Stop();
    audio_thread_.reset();
  }

  media::AudioParameters params_;
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
  RTCMediaConstraints constraints;
  scoped_refptr<WebRtcLocalAudioTrack> track =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
          &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track->audio_source_provider())->SetSinkParamsForTesting(params_);
  track->Start();
  EXPECT_TRUE(track->enabled());

  // Connect a number of network channels to the audio track.
  static const int kNumberOfNetworkChannels = 4;
  for (int i = 0; i < kNumberOfNetworkChannels; ++i) {
    static_cast<webrtc::AudioTrackInterface*>(track.get())->
        GetRenderer()->AddChannel(i);
  }
  scoped_ptr<MockWebRtcAudioCapturerSink> sink(
      new MockWebRtcAudioCapturerSink());
  const media::AudioParameters params = capturer_->audio_parameters();
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*sink, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink,
      CaptureData(kNumberOfNetworkChannels,
                  params.sample_rate(),
                  params.channels(),
                  params.sample_rate() / 100,
                  0,
                  0,
                  false,
                  false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event));
  track->AddSink(sink.get());

  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));
  track->RemoveSink(sink.get());

  EXPECT_CALL(*capturer_source_.get(), Stop()).WillOnce(Return());
  capturer_->Stop();
}

// The same setup as ConnectAndDisconnectOneSink, but enable and disable the
// audio track on the fly. When the audio track is disabled, there is no data
// callback to the sink; when the audio track is enabled, there comes data
// callback.
// TODO(xians): Enable this test after resolving the racing issue that TSAN
// reports on MediaStreamTrack::enabled();
TEST_F(WebRtcLocalAudioTrackTest, DISABLED_DisableEnableAudioTrack) {
  EXPECT_CALL(*capturer_source_.get(), Start()).WillOnce(Return());
  RTCMediaConstraints constraints;
  scoped_refptr<WebRtcLocalAudioTrack> track =
    WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
        &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track->audio_source_provider())->SetSinkParamsForTesting(params_);
  track->Start();
  static_cast<webrtc::AudioTrackInterface*>(track.get())->
      GetRenderer()->AddChannel(0);
  EXPECT_TRUE(track->enabled());
  EXPECT_TRUE(track->set_enabled(false));
  scoped_ptr<MockWebRtcAudioCapturerSink> sink(
      new MockWebRtcAudioCapturerSink());
  const media::AudioParameters params = capturer_->audio_parameters();
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*sink, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink,
              CaptureData(1,
                          params.sample_rate(),
                          params.channels(),
                          params.sample_rate() / 100,
                          0,
                          0,
                          false,
                          false)).Times(0);
  track->AddSink(sink.get());
  EXPECT_FALSE(event.TimedWait(TestTimeouts::tiny_timeout()));

  event.Reset();
  EXPECT_CALL(*sink,
              CaptureData(1,
                          params.sample_rate(),
                          params.channels(),
                          params.sample_rate() / 100,
                          0,
                          0,
                          false,
                          false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event));
  EXPECT_TRUE(track->set_enabled(true));
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));
  track->RemoveSink(sink.get());

  EXPECT_CALL(*capturer_source_.get(), Stop()).WillOnce(Return());
  capturer_->Stop();
  track = NULL;
}

// Create multiple audio tracks and enable/disable them, verify that the audio
// callbacks appear/disappear.
// Flaky due to a data race, see http://crbug.com/295418
TEST_F(WebRtcLocalAudioTrackTest, DISABLED_MultipleAudioTracks) {
  EXPECT_CALL(*capturer_source_.get(), Start()).WillOnce(Return());
  RTCMediaConstraints constraints;
  scoped_refptr<WebRtcLocalAudioTrack> track_1 =
    WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
        &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_1->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_1->Start();
  static_cast<webrtc::AudioTrackInterface*>(track_1.get())->
      GetRenderer()->AddChannel(0);
  EXPECT_TRUE(track_1->enabled());
  scoped_ptr<MockWebRtcAudioCapturerSink> sink_1(
      new MockWebRtcAudioCapturerSink());
  const media::AudioParameters params = capturer_->audio_parameters();
  base::WaitableEvent event_1(false, false);
  EXPECT_CALL(*sink_1, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink_1,
      CaptureData(1,
                  params.sample_rate(),
                  params.channels(),
                  params.sample_rate() / 100,
                  0,
                  0,
                  false,
                  false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event_1));
  track_1->AddSink(sink_1.get());
  EXPECT_TRUE(event_1.TimedWait(TestTimeouts::tiny_timeout()));

  scoped_refptr<WebRtcLocalAudioTrack> track_2 =
    WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
        &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_2->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_2->Start();
  static_cast<webrtc::AudioTrackInterface*>(track_2.get())->
      GetRenderer()->AddChannel(1);
  EXPECT_TRUE(track_2->enabled());

  // Verify both |sink_1| and |sink_2| get data.
  event_1.Reset();
  base::WaitableEvent event_2(false, false);

  scoped_ptr<MockWebRtcAudioCapturerSink> sink_2(
        new MockWebRtcAudioCapturerSink());
  EXPECT_CALL(*sink_2, SetCaptureFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink_1,
      CaptureData(1,
                  params.sample_rate(),
                  params.channels(),
                  params.sample_rate() / 100,
                  0,
                  0,
                  false,
                  false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event_1));
  EXPECT_CALL(*sink_2,
      CaptureData(1,
                  params.sample_rate(),
                  params.channels(),
                  params.sample_rate() / 100,
                  0,
                  0,
                  false,
                  false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event_2));
  track_2->AddSink(sink_2.get());
  EXPECT_TRUE(event_1.TimedWait(TestTimeouts::tiny_timeout()));
  EXPECT_TRUE(event_2.TimedWait(TestTimeouts::tiny_timeout()));

  track_1->RemoveSink(sink_1.get());
  track_1->Stop();
  track_1 = NULL;

  track_2->RemoveSink(sink_2.get());
  track_2->Stop();
  track_2 = NULL;

  EXPECT_CALL(*capturer_source_.get(), Stop()).WillOnce(Return());
  capturer_->Stop();
}


// Start one track and verify the capturer is correctly starting its source.
// And it should be fine to not to call Stop() explicitly.
TEST_F(WebRtcLocalAudioTrackTest, StartOneAudioTrack) {
  EXPECT_CALL(*capturer_source_.get(), Start()).Times(1);
  RTCMediaConstraints constraints;
  scoped_refptr<WebRtcLocalAudioTrack> track =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
          &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track->audio_source_provider())->SetSinkParamsForTesting(params_);
  track->Start();

  // When the track goes away, it will automatically stop the
  // |capturer_source_|.
  EXPECT_CALL(*capturer_source_.get(), Stop());
  capturer_->Stop();
  track = NULL;
}

// Start/Stop tracks and verify the capturer is correctly starting/stopping
// its source.
TEST_F(WebRtcLocalAudioTrackTest, StartAndStopAudioTracks) {
  // Starting the first audio track will start the |capturer_source_|.
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*capturer_source_.get(), Start()).WillOnce(SignalEvent(&event));
  RTCMediaConstraints constraints;
  scoped_refptr<WebRtcLocalAudioTrack> track_1 =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
          &constraints);
  static_cast<webrtc::AudioTrackInterface*>(track_1.get())->
      GetRenderer()->AddChannel(0);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_1->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_1->Start();
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));

  // Verify the data flow by connecting the sink to |track_1|.
  scoped_ptr<MockWebRtcAudioCapturerSink> sink(
      new MockWebRtcAudioCapturerSink());
  event.Reset();
  EXPECT_CALL(*sink, CaptureData(_, _, _, _, 0, 0, false, false))
      .Times(AnyNumber()).WillRepeatedly(Return());
  EXPECT_CALL(*sink, SetCaptureFormat(_)).Times(1);
  track_1->AddSink(sink.get());

  // Start the second audio track will not start the |capturer_source_|
  // since it has been started.
  EXPECT_CALL(*capturer_source_.get(), Start()).Times(0);
  scoped_refptr<WebRtcLocalAudioTrack> track_2 =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
          &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_2->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_2->Start();
  static_cast<webrtc::AudioTrackInterface*>(track_2.get())->
      GetRenderer()->AddChannel(1);

  // Stop the capturer will clear up the track lists in the capturer.
  EXPECT_CALL(*capturer_source_.get(), Stop());
  capturer_->Stop();

  // Adding a new track to the capturer.
  EXPECT_CALL(*sink, SetCaptureFormat(_)).Times(1);
  track_2->AddSink(sink.get());

  // Stop the capturer again will not trigger stopping the source of the
  // capturer again..
  event.Reset();
  EXPECT_CALL(*capturer_source_.get(), Stop()).Times(0);
  capturer_->Stop();
}

// Set new source to the existing capturer.
TEST_F(WebRtcLocalAudioTrackTest, SetNewSourceForCapturerAfterStartTrack) {
  // Setup the audio track and start the track.
  EXPECT_CALL(*capturer_source_.get(), Start()).Times(1);
  RTCMediaConstraints constraints;
  scoped_refptr<WebRtcLocalAudioTrack> track =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
          &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track->audio_source_provider())->SetSinkParamsForTesting(params_);
  track->Start();

  // Setting new source to the capturer and the track should still get packets.
  scoped_refptr<MockCapturerSource> new_source(new MockCapturerSource());
  EXPECT_CALL(*capturer_source_.get(), Stop());
  EXPECT_CALL(*new_source.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*new_source.get(), Initialize(_, capturer_.get(), 0))
      .WillOnce(Return());
  EXPECT_CALL(*new_source.get(), Start()).WillOnce(Return());
  capturer_->SetCapturerSource(new_source,
                               params_.channel_layout(),
                               params_.sample_rate());

  // Stop the track.
  EXPECT_CALL(*new_source.get(), Stop());
  capturer_->Stop();
}

// Create a new capturer with new source, connect it to a new audio track.
TEST_F(WebRtcLocalAudioTrackTest, ConnectTracksToDifferentCapturers) {
  // Setup the first audio track and start it.
  EXPECT_CALL(*capturer_source_.get(), Start()).Times(1);
  RTCMediaConstraints constraints;
  scoped_refptr<WebRtcLocalAudioTrack> track_1 =
      WebRtcLocalAudioTrack::Create(std::string(), capturer_, NULL, NULL,
          &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_1->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_1->Start();

  // Connect a number of network channels to the |track_1|.
  static const int kNumberOfNetworkChannelsForTrack1 = 2;
  for (int i = 0; i < kNumberOfNetworkChannelsForTrack1; ++i) {
    static_cast<webrtc::AudioTrackInterface*>(track_1.get())->
        GetRenderer()->AddChannel(i);
  }
  // Verify the data flow by connecting the |sink_1| to |track_1|.
  scoped_ptr<MockWebRtcAudioCapturerSink> sink_1(
      new MockWebRtcAudioCapturerSink());
  EXPECT_CALL(
      *sink_1.get(),
      CaptureData(
          kNumberOfNetworkChannelsForTrack1, 48000, 2, _, 0, 0, false, false))
      .Times(AnyNumber()).WillRepeatedly(Return());
  EXPECT_CALL(*sink_1.get(), SetCaptureFormat(_)).Times(1);
  track_1->AddSink(sink_1.get());

  // Create a new capturer with new source with different audio format.
  scoped_refptr<WebRtcAudioCapturer> new_capturer(
      WebRtcAudioCapturer::CreateCapturer());
  scoped_refptr<MockCapturerSource> new_source(new MockCapturerSource());
  EXPECT_CALL(*new_source.get(), Initialize(_, new_capturer.get(), 0))
      .WillOnce(Return());
  EXPECT_CALL(*new_source.get(), SetAutomaticGainControl(true))
      .WillOnce(Return());
  new_capturer->SetCapturerSource(new_source,
                                  media::CHANNEL_LAYOUT_MONO,
                                  44100);

  // Start the audio thread used by the new source.
  scoped_ptr<FakeAudioThread> audio_thread(new FakeAudioThread(new_capturer));
  audio_thread->Start();

  // Setup the second audio track, connect it to the new capturer and start it.
  EXPECT_CALL(*new_source.get(), Start()).Times(1);
  scoped_refptr<WebRtcLocalAudioTrack> track_2 =
      WebRtcLocalAudioTrack::Create(std::string(), new_capturer, NULL, NULL,
          &constraints);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_2->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_2->Start();

  // Connect a number of network channels to the |track_2|.
  static const int kNumberOfNetworkChannelsForTrack2 = 3;
  for (int i = 0; i < kNumberOfNetworkChannelsForTrack2; ++i) {
    static_cast<webrtc::AudioTrackInterface*>(track_2.get())->
        GetRenderer()->AddChannel(i);
  }
  // Verify the data flow by connecting the |sink_2| to |track_2|.
  scoped_ptr<MockWebRtcAudioCapturerSink> sink_2(
      new MockWebRtcAudioCapturerSink());
  EXPECT_CALL(
      *sink_2,
      CaptureData(
          kNumberOfNetworkChannelsForTrack2, 44100, 1, _, 0, 0, false, false))
      .Times(AnyNumber()).WillRepeatedly(Return());
  EXPECT_CALL(*sink_2, SetCaptureFormat(_)).Times(1);
  track_2->AddSink(sink_2.get());

  // Stopping the new source will stop the second track.
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*new_source.get(), Stop()).Times(1).WillOnce(SignalEvent(&event));
  new_capturer->Stop();
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));
  audio_thread->Stop();
  audio_thread.reset();

  // Stop the capturer of the first audio track.
  EXPECT_CALL(*capturer_source_.get(), Stop());
  capturer_->Stop();
}

}  // namespace content
