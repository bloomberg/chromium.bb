// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
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
  FakeAudioThread(const scoped_refptr<WebRtcAudioCapturer>& capturer,
                  const media::AudioParameters& params)
    : capturer_(capturer),
      thread_(),
      closure_(false, false) {
    DCHECK(capturer.get());
    audio_bus_ = media::AudioBus::Create(params);
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
  explicit MockCapturerSource(WebRtcAudioCapturer* capturer)
      : capturer_(capturer) {}
  MOCK_METHOD3(OnInitialize, void(const media::AudioParameters& params,
                                  CaptureCallback* callback,
                                  int session_id));
  MOCK_METHOD0(OnStart, void());
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetAutomaticGainControl, void(bool enable));

  virtual void Initialize(const media::AudioParameters& params,
                          CaptureCallback* callback,
                          int session_id) OVERRIDE {
    DCHECK(params.IsValid());
    params_ = params;
    OnInitialize(params, callback, session_id);
  }
  virtual void Start() OVERRIDE {
    audio_thread_.reset(new FakeAudioThread(capturer_, params_));
    audio_thread_->Start();
    OnStart();
  }
  virtual void Stop() OVERRIDE {
    audio_thread_->Stop();
    audio_thread_.reset();
    OnStop();
  }
 protected:
  virtual ~MockCapturerSource() {}

 private:
  scoped_ptr<FakeAudioThread> audio_thread_;
  WebRtcAudioCapturer* capturer_;
  media::AudioParameters params_;
};

// TODO(xians): Use MediaStreamAudioSink.
class MockMediaStreamAudioSink : public PeerConnectionAudioSink {
 public:
  MockMediaStreamAudioSink() {}
  ~MockMediaStreamAudioSink() {}
  int OnData(const int16* audio_data,
             int sample_rate,
             int number_of_channels,
             int number_of_frames,
             const std::vector<int>& channels,
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
  MOCK_METHOD1(OnSetFormat, void(const media::AudioParameters& params));
};

}  // namespace

class WebRtcLocalAudioTrackTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                  media::CHANNEL_LAYOUT_STEREO, 2, 0, 48000, 16, 480);
    blink::WebMediaConstraints constraints;
    StreamDeviceInfo device(MEDIA_DEVICE_AUDIO_CAPTURE,
                            std::string(), std::string());
    capturer_ = WebRtcAudioCapturer::CreateCapturer(-1, device,
                                                    constraints, NULL);
    capturer_source_ = new MockCapturerSource(capturer_.get());
    EXPECT_CALL(*capturer_source_.get(), OnInitialize(_, capturer_.get(), -1))
        .WillOnce(Return());
    capturer_->SetCapturerSourceForTesting(capturer_source_, params_);
  }

  media::AudioParameters params_;
  scoped_refptr<MockCapturerSource> capturer_source_;
  scoped_refptr<WebRtcAudioCapturer> capturer_;
};

// Creates a capturer and audio track, fakes its audio thread, and
// connect/disconnect the sink to the audio track on the fly, the sink should
// get data callback when the track is connected to the capturer but not when
// the track is disconnected from the capturer.
TEST_F(WebRtcLocalAudioTrackTest, ConnectAndDisconnectOneSink) {
  EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*capturer_source_.get(), OnStart());
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track(
      new WebRtcLocalAudioTrack(adapter, capturer_, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track->audio_source_provider())->SetSinkParamsForTesting(params_);
  track->Start();
  EXPECT_TRUE(track->GetAudioAdapter()->enabled());

  // Connect a number of network channels to the audio track.
  static const int kNumberOfNetworkChannels = 4;
  for (int i = 0; i < kNumberOfNetworkChannels; ++i) {
    static_cast<webrtc::AudioTrackInterface*>(
        adapter.get())->GetRenderer()->AddChannel(i);
  }
  scoped_ptr<MockMediaStreamAudioSink> sink(new MockMediaStreamAudioSink());
  const media::AudioParameters params = capturer_->source_audio_parameters();
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*sink, OnSetFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink,
      CaptureData(kNumberOfNetworkChannels,
                  params.sample_rate(),
                  params.channels(),
                  params.sample_rate() / 100,
                  0,
                  0,
                  true,
                  false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event));
  track->AddSink(sink.get());

  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));
  track->RemoveSink(sink.get());

  EXPECT_CALL(*capturer_source_.get(), OnStop()).WillOnce(Return());
  capturer_->Stop();
}

// The same setup as ConnectAndDisconnectOneSink, but enable and disable the
// audio track on the fly. When the audio track is disabled, there is no data
// callback to the sink; when the audio track is enabled, there comes data
// callback.
// TODO(xians): Enable this test after resolving the racing issue that TSAN
// reports on MediaStreamTrack::enabled();
TEST_F(WebRtcLocalAudioTrackTest,  DISABLED_DisableEnableAudioTrack) {
  EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*capturer_source_.get(), OnStart());
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track(
      new WebRtcLocalAudioTrack(adapter, capturer_, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track->audio_source_provider())->SetSinkParamsForTesting(params_);
  track->Start();
  static_cast<webrtc::AudioTrackInterface*>(
      adapter.get())->GetRenderer()->AddChannel(0);
  EXPECT_TRUE(track->GetAudioAdapter()->enabled());
  EXPECT_TRUE(track->GetAudioAdapter()->set_enabled(false));
  scoped_ptr<MockMediaStreamAudioSink> sink(new MockMediaStreamAudioSink());
  const media::AudioParameters params = capturer_->source_audio_parameters();
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*sink, OnSetFormat(_)).Times(1);
  EXPECT_CALL(*sink,
              CaptureData(1,
                          params.sample_rate(),
                          params.channels(),
                          params.sample_rate() / 100,
                          0,
                          0,
                          true,
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
                          true,
                          false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event));
  EXPECT_TRUE(track->GetAudioAdapter()->set_enabled(true));
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));
  track->RemoveSink(sink.get());

  EXPECT_CALL(*capturer_source_.get(), OnStop()).WillOnce(Return());
  capturer_->Stop();
  track.reset();
}

// Create multiple audio tracks and enable/disable them, verify that the audio
// callbacks appear/disappear.
// Flaky due to a data race, see http://crbug.com/295418
TEST_F(WebRtcLocalAudioTrackTest, DISABLED_MultipleAudioTracks) {
  EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*capturer_source_.get(), OnStart());
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_1(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track_1(
    new WebRtcLocalAudioTrack(adapter_1, capturer_, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_1->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_1->Start();
  static_cast<webrtc::AudioTrackInterface*>(
      adapter_1.get())->GetRenderer()->AddChannel(0);
  EXPECT_TRUE(track_1->GetAudioAdapter()->enabled());
  scoped_ptr<MockMediaStreamAudioSink> sink_1(new MockMediaStreamAudioSink());
  const media::AudioParameters params = capturer_->source_audio_parameters();
  base::WaitableEvent event_1(false, false);
  EXPECT_CALL(*sink_1, OnSetFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink_1,
      CaptureData(1,
                  params.sample_rate(),
                  params.channels(),
                  params.sample_rate() / 100,
                  0,
                  0,
                  true,
                  false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event_1));
  track_1->AddSink(sink_1.get());
  EXPECT_TRUE(event_1.TimedWait(TestTimeouts::tiny_timeout()));

  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_2(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track_2(
    new WebRtcLocalAudioTrack(adapter_2, capturer_, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_2->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_2->Start();
  static_cast<webrtc::AudioTrackInterface*>(
      adapter_2.get())->GetRenderer()->AddChannel(1);
  EXPECT_TRUE(track_2->GetAudioAdapter()->enabled());

  // Verify both |sink_1| and |sink_2| get data.
  event_1.Reset();
  base::WaitableEvent event_2(false, false);

  scoped_ptr<MockMediaStreamAudioSink> sink_2(new MockMediaStreamAudioSink());
  EXPECT_CALL(*sink_2, OnSetFormat(_)).WillOnce(Return());
  EXPECT_CALL(*sink_1,
      CaptureData(1,
                  params.sample_rate(),
                  params.channels(),
                  params.sample_rate() / 100,
                  0,
                  0,
                  true,
                  false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event_1));
  EXPECT_CALL(*sink_2,
      CaptureData(1,
                  params.sample_rate(),
                  params.channels(),
                  params.sample_rate() / 100,
                  0,
                  0,
                  true,
                  false)).Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&event_2));
  track_2->AddSink(sink_2.get());
  EXPECT_TRUE(event_1.TimedWait(TestTimeouts::tiny_timeout()));
  EXPECT_TRUE(event_2.TimedWait(TestTimeouts::tiny_timeout()));

  track_1->RemoveSink(sink_1.get());
  track_1->Stop();
  track_1.reset();

  EXPECT_CALL(*capturer_source_.get(), OnStop()).WillOnce(Return());
  track_2->RemoveSink(sink_2.get());
  track_2->Stop();
  track_2.reset();

  capturer_->Stop();
}


// Start one track and verify the capturer is correctly starting its source.
// And it should be fine to not to call Stop() explicitly.
TEST_F(WebRtcLocalAudioTrackTest, StartOneAudioTrack) {
  EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*capturer_source_.get(), OnStart());
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track(
      new WebRtcLocalAudioTrack(adapter, capturer_, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track->audio_source_provider())->SetSinkParamsForTesting(params_);
  track->Start();

  // When the track goes away, it will automatically stop the
  // |capturer_source_|.
  EXPECT_CALL(*capturer_source_.get(), OnStop());
  capturer_->Stop();
  track.reset();
}

// Start/Stop tracks and verify the capturer is correctly starting/stopping
// its source.
TEST_F(WebRtcLocalAudioTrackTest, StartAndStopAudioTracks) {
  // Starting the first audio track will start the |capturer_source_|.
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*capturer_source_.get(), OnStart()).WillOnce(SignalEvent(&event));
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_1(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track_1(
      new WebRtcLocalAudioTrack(adapter_1, capturer_, NULL));
  static_cast<webrtc::AudioTrackInterface*>(
      adapter_1.get())->GetRenderer()->AddChannel(0);
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_1->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_1->Start();
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));

  // Verify the data flow by connecting the sink to |track_1|.
  scoped_ptr<MockMediaStreamAudioSink> sink(new MockMediaStreamAudioSink());
  event.Reset();
  EXPECT_CALL(*sink, OnSetFormat(_)).WillOnce(SignalEvent(&event));
  EXPECT_CALL(*sink, CaptureData(_, _, _, _, 0, 0, true, false))
      .Times(AnyNumber()).WillRepeatedly(Return());
  track_1->AddSink(sink.get());
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));

  // Start the second audio track will not start the |capturer_source_|
  // since it has been started.
  EXPECT_CALL(*capturer_source_.get(), OnStart()).Times(0);
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_2(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track_2(
      new WebRtcLocalAudioTrack(adapter_2, capturer_, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_2->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_2->Start();
  static_cast<webrtc::AudioTrackInterface*>(
      adapter_2.get())->GetRenderer()->AddChannel(1);

  // Stop the capturer will clear up the track lists in the capturer.
  EXPECT_CALL(*capturer_source_.get(), OnStop());
  capturer_->Stop();

  // Adding a new track to the capturer.
  track_2->AddSink(sink.get());
  EXPECT_CALL(*sink, OnSetFormat(_)).Times(0);

  // Stop the capturer again will not trigger stopping the source of the
  // capturer again..
  event.Reset();
  EXPECT_CALL(*capturer_source_.get(), OnStop()).Times(0);
  capturer_->Stop();
}

// Create a new capturer with new source, connect it to a new audio track.
TEST_F(WebRtcLocalAudioTrackTest, ConnectTracksToDifferentCapturers) {
  // Setup the first audio track and start it.
  EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*capturer_source_.get(), OnStart());
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_1(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track_1(
      new WebRtcLocalAudioTrack(adapter_1, capturer_, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_1->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_1->Start();

  // Connect a number of network channels to the |track_1|.
  static const int kNumberOfNetworkChannelsForTrack1 = 2;
  for (int i = 0; i < kNumberOfNetworkChannelsForTrack1; ++i) {
    static_cast<webrtc::AudioTrackInterface*>(
        adapter_1.get())->GetRenderer()->AddChannel(i);
  }
  // Verify the data flow by connecting the |sink_1| to |track_1|.
  scoped_ptr<MockMediaStreamAudioSink> sink_1(new MockMediaStreamAudioSink());
  EXPECT_CALL(
      *sink_1.get(),
      CaptureData(
          kNumberOfNetworkChannelsForTrack1, 48000, 2, _, 0, 0, true, false))
      .Times(AnyNumber()).WillRepeatedly(Return());
  EXPECT_CALL(*sink_1.get(), OnSetFormat(_)).Times(AnyNumber());
  track_1->AddSink(sink_1.get());

  // Create a new capturer with new source with different audio format.
  blink::WebMediaConstraints constraints;
  StreamDeviceInfo device(MEDIA_DEVICE_AUDIO_CAPTURE,
                          std::string(), std::string());
  scoped_refptr<WebRtcAudioCapturer> new_capturer(
      WebRtcAudioCapturer::CreateCapturer(-1, device, constraints, NULL));
  scoped_refptr<MockCapturerSource> new_source(
      new MockCapturerSource(new_capturer.get()));
  EXPECT_CALL(*new_source.get(), OnInitialize(_, new_capturer.get(), -1));
  media::AudioParameters new_param(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_MONO, 44100, 16, 441);
  new_capturer->SetCapturerSourceForTesting(new_source, new_param);

  // Setup the second audio track, connect it to the new capturer and start it.
  EXPECT_CALL(*new_source.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*new_source.get(), OnStart());
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_2(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track_2(
      new WebRtcLocalAudioTrack(adapter_2, new_capturer, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track_2->audio_source_provider())->SetSinkParamsForTesting(params_);
  track_2->Start();

  // Connect a number of network channels to the |track_2|.
  static const int kNumberOfNetworkChannelsForTrack2 = 3;
  for (int i = 0; i < kNumberOfNetworkChannelsForTrack2; ++i) {
    static_cast<webrtc::AudioTrackInterface*>(
        adapter_2.get())->GetRenderer()->AddChannel(i);
  }
  // Verify the data flow by connecting the |sink_2| to |track_2|.
  scoped_ptr<MockMediaStreamAudioSink> sink_2(new MockMediaStreamAudioSink());
  base::WaitableEvent event(false, false);
  EXPECT_CALL(
      *sink_2,
      CaptureData(
          kNumberOfNetworkChannelsForTrack2, new_param.sample_rate(),
          new_param.channels(), _, 0, 0, true, false))
      .Times(AnyNumber()).WillRepeatedly(Return());
  EXPECT_CALL(*sink_2, OnSetFormat(_)).WillOnce(SignalEvent(&event));
  track_2->AddSink(sink_2.get());
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));

  // Stopping the new source will stop the second track.
  event.Reset();
  EXPECT_CALL(*new_source.get(), OnStop())
      .Times(1).WillOnce(SignalEvent(&event));
  new_capturer->Stop();
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));

  // Stop the capturer of the first audio track.
  EXPECT_CALL(*capturer_source_.get(), OnStop());
  capturer_->Stop();
}


// Make sure a audio track can deliver packets with a buffer size smaller than
// 10ms when it is not connected with a peer connection.
TEST_F(WebRtcLocalAudioTrackTest, TrackWorkWithSmallBufferSize) {
  // Setup a capturer which works with a buffer size smaller than 10ms.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 128);

  // Create a capturer with new source which works with the format above.
  blink::WebMediaConstraints constraints;
  scoped_refptr<WebRtcAudioCapturer> capturer(
      WebRtcAudioCapturer::CreateCapturer(
          -1,
          StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE,
                           "", "", params.sample_rate(),
                           params.channel_layout(),
                           params.frames_per_buffer()),
          constraints,
          NULL));
  scoped_refptr<MockCapturerSource> source(
      new MockCapturerSource(capturer.get()));
  EXPECT_CALL(*source.get(), OnInitialize(_, capturer.get(), -1));
  capturer->SetCapturerSourceForTesting(source, params);

  // Setup a audio track, connect it to the capturer and start it.
  EXPECT_CALL(*source.get(), SetAutomaticGainControl(true));
  EXPECT_CALL(*source.get(), OnStart());
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> track(
      new WebRtcLocalAudioTrack(adapter, capturer, NULL));
  static_cast<WebRtcLocalAudioSourceProvider*>(
      track->audio_source_provider())->SetSinkParamsForTesting(params);
  track->Start();

  // Verify the data flow by connecting the |sink| to |track|.
  scoped_ptr<MockMediaStreamAudioSink> sink(new MockMediaStreamAudioSink());
  base::WaitableEvent event(false, false);
  EXPECT_CALL(*sink, OnSetFormat(_)).Times(1);
  // Verify the sinks are getting the packets with an expecting buffer size.
#if defined(OS_ANDROID)
  const int expected_buffer_size = params.sample_rate() / 100;
#else
  const int expected_buffer_size = params.frames_per_buffer();
#endif
  EXPECT_CALL(*sink, CaptureData(
      0, params.sample_rate(), params.channels(), expected_buffer_size,
      0, 0, true, false))
      .Times(AtLeast(1)).WillRepeatedly(SignalEvent(&event));
  track->AddSink(sink.get());
  EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout()));

  // Stopping the new source will stop the second track.
  EXPECT_CALL(*source, OnStop()).Times(1);
  capturer->Stop();
}

}  // namespace content
