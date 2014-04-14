// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_processor.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_fifo.h"
#include "media/base/channel_layout.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"

namespace content {

namespace {

using webrtc::AudioProcessing;
using webrtc::MediaConstraintsInterface;

#if defined(OS_ANDROID)
const int kAudioProcessingSampleRate = 16000;
#else
const int kAudioProcessingSampleRate = 32000;
#endif
const int kAudioProcessingNumberOfChannels = 1;

const int kMaxNumberOfBuffersInFifo = 2;

// Used by UMA histograms and entries shouldn't be re-ordered or removed.
enum AudioTrackProcessingStates {
  AUDIO_PROCESSING_ENABLED = 0,
  AUDIO_PROCESSING_DISABLED,
  AUDIO_PROCESSING_IN_WEBRTC,
  AUDIO_PROCESSING_MAX
};

void RecordProcessingState(AudioTrackProcessingStates state) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioTrackProcessingStates",
                            state, AUDIO_PROCESSING_MAX);
}

}  // namespace

class MediaStreamAudioProcessor::MediaStreamAudioConverter
    : public media::AudioConverter::InputCallback {
 public:
  MediaStreamAudioConverter(const media::AudioParameters& source_params,
                            const media::AudioParameters& sink_params)
     : source_params_(source_params),
       sink_params_(sink_params),
       audio_converter_(source_params, sink_params_, false) {
    // An instance of MediaStreamAudioConverter may be created in the main
    // render thread and used in the audio thread, for example, the
    // |MediaStreamAudioProcessor::capture_converter_|.
    thread_checker_.DetachFromThread();
    audio_converter_.AddInput(this);
    // Create and initialize audio fifo and audio bus wrapper.
    // The size of the FIFO should be at least twice of the source buffer size
    // or twice of the sink buffer size.
    int buffer_size = std::max(
        kMaxNumberOfBuffersInFifo * source_params_.frames_per_buffer(),
        kMaxNumberOfBuffersInFifo * sink_params_.frames_per_buffer());
    fifo_.reset(new media::AudioFifo(source_params_.channels(), buffer_size));
    // TODO(xians): Use CreateWrapper to save one memcpy.
    audio_wrapper_ = media::AudioBus::Create(sink_params_.channels(),
                                             sink_params_.frames_per_buffer());
  }

  virtual ~MediaStreamAudioConverter() {
    audio_converter_.RemoveInput(this);
  }

  void Push(media::AudioBus* audio_source) {
    // Called on the audio thread, which is the capture audio thread for
    // |MediaStreamAudioProcessor::capture_converter_|, and render audio thread
    // for |MediaStreamAudioProcessor::render_converter_|.
    // And it must be the same thread as calling Convert().
    DCHECK(thread_checker_.CalledOnValidThread());
    fifo_->Push(audio_source);
  }

  bool Convert(webrtc::AudioFrame* out) {
    // Called on the audio thread, which is the capture audio thread for
    // |MediaStreamAudioProcessor::capture_converter_|, and render audio thread
    // for |MediaStreamAudioProcessor::render_converter_|.
    DCHECK(thread_checker_.CalledOnValidThread());
    // Return false if there is not enough data in the FIFO, this happens when
    // fifo_->frames() / source_params_.sample_rate() is less than
    // sink_params.frames_per_buffer() / sink_params.sample_rate().
    if (fifo_->frames() * sink_params_.sample_rate() <
        sink_params_.frames_per_buffer() * source_params_.sample_rate()) {
      return false;
    }

    // Convert data to the output format, this will trigger ProvideInput().
    audio_converter_.Convert(audio_wrapper_.get());

    // TODO(xians): Figure out a better way to handle the interleaved and
    // deinterleaved format switching.
    DCHECK_EQ(audio_wrapper_->frames(), sink_params_.frames_per_buffer());
    audio_wrapper_->ToInterleaved(audio_wrapper_->frames(),
                                  sink_params_.bits_per_sample() / 8,
                                  out->data_);

    out->samples_per_channel_ = sink_params_.frames_per_buffer();
    out->sample_rate_hz_ = sink_params_.sample_rate();
    out->speech_type_ = webrtc::AudioFrame::kNormalSpeech;
    out->vad_activity_ = webrtc::AudioFrame::kVadUnknown;
    out->num_channels_ = sink_params_.channels();

    return true;
  }

  const media::AudioParameters& source_parameters() const {
    return source_params_;
  }
  const media::AudioParameters& sink_parameters() const {
    return sink_params_;
  }

 private:
  // AudioConverter::InputCallback implementation.
  virtual double ProvideInput(media::AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) OVERRIDE {
    // Called on realtime audio thread.
    // TODO(xians): Figure out why the first Convert() triggers ProvideInput
    // two times.
    if (fifo_->frames() < audio_bus->frames())
      return 0;

    fifo_->Consume(audio_bus, 0, audio_bus->frames());

    // Return 1.0 to indicate no volume scaling on the data.
    return 1.0;
  }

  base::ThreadChecker thread_checker_;
  const media::AudioParameters source_params_;
  const media::AudioParameters sink_params_;

  // TODO(xians): consider using SincResampler to save some memcpy.
  // Handles mixing and resampling between input and output parameters.
  media::AudioConverter audio_converter_;
  scoped_ptr<media::AudioBus> audio_wrapper_;
  scoped_ptr<media::AudioFifo> fifo_;
};

MediaStreamAudioProcessor::MediaStreamAudioProcessor(
    const blink::WebMediaConstraints& constraints,
    int effects,
    MediaStreamType type,
    WebRtcPlayoutDataSource* playout_data_source)
    : render_delay_ms_(0),
      playout_data_source_(playout_data_source),
      audio_mirroring_(false),
      typing_detected_(false) {
  capture_thread_checker_.DetachFromThread();
  render_thread_checker_.DetachFromThread();
  InitializeAudioProcessingModule(constraints, effects, type);
}

MediaStreamAudioProcessor::~MediaStreamAudioProcessor() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  StopAudioProcessing();
}

void MediaStreamAudioProcessor::OnCaptureFormatChanged(
    const media::AudioParameters& source_params) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more PushCaptureData() and ProcessAndConsumeData() callbacks
  // on the capture thread.
  InitializeCaptureConverter(source_params);

  // Reset the |capture_thread_checker_| since the capture data will come from
  // a new capture thread.
  capture_thread_checker_.DetachFromThread();
}

void MediaStreamAudioProcessor::PushCaptureData(media::AudioBus* audio_source) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(audio_source->channels(),
            capture_converter_->source_parameters().channels());
  DCHECK_EQ(audio_source->frames(),
            capture_converter_->source_parameters().frames_per_buffer());

  if (audio_mirroring_ &&
      capture_converter_->source_parameters().channel_layout() ==
          media::CHANNEL_LAYOUT_STEREO) {
    // Swap the first and second channels.
    audio_source->SwapChannels(0, 1);
  }

  capture_converter_->Push(audio_source);
}

bool MediaStreamAudioProcessor::ProcessAndConsumeData(
    base::TimeDelta capture_delay, int volume, bool key_pressed,
    int* new_volume, int16** out) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessAndConsumeData");

  if (!capture_converter_->Convert(&capture_frame_))
    return false;

  *new_volume = ProcessData(&capture_frame_, capture_delay, volume,
                            key_pressed);
  *out = capture_frame_.data_;

  return true;
}

const media::AudioParameters& MediaStreamAudioProcessor::InputFormat() const {
  return capture_converter_->source_parameters();
}

const media::AudioParameters& MediaStreamAudioProcessor::OutputFormat() const {
  return capture_converter_->sink_parameters();
}

void MediaStreamAudioProcessor::StartAecDump(
    const base::PlatformFile& aec_dump_file) {
  if (audio_processing_)
    StartEchoCancellationDump(audio_processing_.get(), aec_dump_file);
}

void MediaStreamAudioProcessor::StopAecDump() {
  if (audio_processing_)
    StopEchoCancellationDump(audio_processing_.get());
}

void MediaStreamAudioProcessor::OnPlayoutData(media::AudioBus* audio_bus,
                                              int sample_rate,
                                              int audio_delay_milliseconds) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
#if defined(OS_ANDROID) || defined(OS_IOS)
  DCHECK(audio_processing_->echo_control_mobile()->is_enabled());
#else
  DCHECK(audio_processing_->echo_cancellation()->is_enabled());
#endif

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::OnPlayoutData");
  DCHECK_LT(audio_delay_milliseconds,
            std::numeric_limits<base::subtle::Atomic32>::max());
  base::subtle::Release_Store(&render_delay_ms_, audio_delay_milliseconds);

  InitializeRenderConverterIfNeeded(sample_rate, audio_bus->channels(),
                                    audio_bus->frames());

  render_converter_->Push(audio_bus);
  while (render_converter_->Convert(&render_frame_))
    audio_processing_->AnalyzeReverseStream(&render_frame_);
}

void MediaStreamAudioProcessor::OnPlayoutDataSourceChanged() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more OnPlayoutData() callback on the render thread.
  render_thread_checker_.DetachFromThread();
  render_converter_.reset();
}

void MediaStreamAudioProcessor::GetStats(AudioProcessorStats* stats) {
  stats->typing_noise_detected =
      (base::subtle::Acquire_Load(&typing_detected_) != false);
  GetAecStats(audio_processing_.get(), stats);
}

void MediaStreamAudioProcessor::InitializeAudioProcessingModule(
    const blink::WebMediaConstraints& constraints, int effects,
    MediaStreamType type) {
  DCHECK(!audio_processing_);

  RTCMediaConstraints native_constraints(constraints);

  // Audio mirroring can be enabled even though audio processing is otherwise
  // disabled.
  audio_mirroring_ = GetPropertyFromConstraints(
      &native_constraints, webrtc::MediaConstraintsInterface::kAudioMirroring);

  if (!IsAudioTrackProcessingEnabled()) {
    RecordProcessingState(AUDIO_PROCESSING_IN_WEBRTC);
    return;
  }

  // Only apply the fixed constraints for gUM of MEDIA_DEVICE_AUDIO_CAPTURE.
  DCHECK(IsAudioMediaType(type));
  if (type == MEDIA_DEVICE_AUDIO_CAPTURE)
    ApplyFixedAudioConstraints(&native_constraints);

  if (effects & media::AudioParameters::ECHO_CANCELLER) {
    // If platform echo canceller is enabled, disable the software AEC.
    native_constraints.AddMandatory(
        MediaConstraintsInterface::kEchoCancellation,
        MediaConstraintsInterface::kValueFalse, true);
  }

#if defined(OS_IOS)
  // On iOS, VPIO provides built-in AEC and AGC.
  const bool enable_aec = false;
  const bool enable_agc = false;
#else
  const bool enable_aec = GetPropertyFromConstraints(
      &native_constraints, MediaConstraintsInterface::kEchoCancellation);
  const bool enable_agc = GetPropertyFromConstraints(
      &native_constraints, webrtc::MediaConstraintsInterface::kAutoGainControl);
#endif

#if defined(OS_IOS) || defined(OS_ANDROID)
  const bool enable_experimental_aec = false;
  const bool enable_typing_detection = false;
#else
  const bool enable_experimental_aec = GetPropertyFromConstraints(
      &native_constraints,
      MediaConstraintsInterface::kExperimentalEchoCancellation);
  const bool enable_typing_detection = GetPropertyFromConstraints(
      &native_constraints, MediaConstraintsInterface::kTypingNoiseDetection);
#endif

  const bool enable_ns = GetPropertyFromConstraints(
      &native_constraints, MediaConstraintsInterface::kNoiseSuppression);
  const bool enable_experimental_ns = GetPropertyFromConstraints(
        &native_constraints,
        MediaConstraintsInterface::kExperimentalNoiseSuppression);
  const bool enable_high_pass_filter = GetPropertyFromConstraints(
      &native_constraints, MediaConstraintsInterface::kHighpassFilter);

  // Return immediately if no audio processing component is enabled.
  if (!enable_aec && !enable_experimental_aec && !enable_ns &&
      !enable_high_pass_filter && !enable_typing_detection && !enable_agc &&
      !enable_experimental_ns) {
    RecordProcessingState(AUDIO_PROCESSING_DISABLED);
    return;
  }

  // Create and configure the webrtc::AudioProcessing.
  audio_processing_.reset(webrtc::AudioProcessing::Create(0));

  // Enable the audio processing components.
  if (enable_aec) {
    EnableEchoCancellation(audio_processing_.get());
    if (enable_experimental_aec)
      EnableExperimentalEchoCancellation(audio_processing_.get());

    if (playout_data_source_)
      playout_data_source_->AddPlayoutSink(this);
  }

  if (enable_ns)
    EnableNoiseSuppression(audio_processing_.get());

  if (enable_experimental_ns)
    EnableExperimentalNoiseSuppression(audio_processing_.get());

  if (enable_high_pass_filter)
    EnableHighPassFilter(audio_processing_.get());

  if (enable_typing_detection) {
    // TODO(xians): Remove this |typing_detector_| after the typing suppression
    // is enabled by default.
    typing_detector_.reset(new webrtc::TypingDetection());
    EnableTypingDetection(audio_processing_.get(), typing_detector_.get());
  }

  if (enable_agc)
    EnableAutomaticGainControl(audio_processing_.get());

  // Configure the audio format the audio processing is running on. This
  // has to be done after all the needed components are enabled.
  CHECK_EQ(0,
           audio_processing_->set_sample_rate_hz(kAudioProcessingSampleRate));
  CHECK_EQ(0, audio_processing_->set_num_channels(
      kAudioProcessingNumberOfChannels, kAudioProcessingNumberOfChannels));

  RecordProcessingState(AUDIO_PROCESSING_ENABLED);
}

void MediaStreamAudioProcessor::InitializeCaptureConverter(
    const media::AudioParameters& source_params) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(source_params.IsValid());

  // Create and initialize audio converter for the source data.
  // When the webrtc AudioProcessing is enabled, the sink format of the
  // converter will be the same as the post-processed data format, which is
  // 32k mono for desktops and 16k mono for Android. When the AudioProcessing
  // is disabled, the sink format will be the same as the source format.
  const int sink_sample_rate = audio_processing_ ?
      kAudioProcessingSampleRate : source_params.sample_rate();
  const media::ChannelLayout sink_channel_layout = audio_processing_ ?
      media::GuessChannelLayout(kAudioProcessingNumberOfChannels) :
      source_params.channel_layout();

  // WebRtc AudioProcessing requires 10ms as its packet size. We use this
  // native size when processing is enabled. While processing is disabled, and
  // the source is running with a buffer size smaller than 10ms buffer, we use
  // same buffer size as the incoming format to avoid extra FIFO for WebAudio.
  int sink_buffer_size =  sink_sample_rate / 100;
  if (!audio_processing_ &&
      source_params.frames_per_buffer() < sink_buffer_size) {
    sink_buffer_size = source_params.frames_per_buffer();
  }

  media::AudioParameters sink_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, sink_channel_layout,
      sink_sample_rate, 16, sink_buffer_size);
  capture_converter_.reset(
      new MediaStreamAudioConverter(source_params, sink_params));
}

void MediaStreamAudioProcessor::InitializeRenderConverterIfNeeded(
    int sample_rate, int number_of_channels, int frames_per_buffer) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  // TODO(xians): Figure out if we need to handle the buffer size change.
  if (render_converter_.get() &&
      render_converter_->source_parameters().sample_rate() == sample_rate &&
      render_converter_->source_parameters().channels() == number_of_channels) {
    // Do nothing if the |render_converter_| has been setup properly.
    return;
  }

  // Create and initialize audio converter for the render data.
  // webrtc::AudioProcessing accepts the same format as what it uses to process
  // capture data, which is 32k mono for desktops and 16k mono for Android.
  media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(number_of_channels), sample_rate, 16,
      frames_per_buffer);
  media::AudioParameters sink_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_MONO, kAudioProcessingSampleRate, 16,
      kAudioProcessingSampleRate / 100);
  render_converter_.reset(
      new MediaStreamAudioConverter(source_params, sink_params));
  render_data_bus_ = media::AudioBus::Create(number_of_channels,
                                             frames_per_buffer);
}

int MediaStreamAudioProcessor::ProcessData(webrtc::AudioFrame* audio_frame,
                                           base::TimeDelta capture_delay,
                                           int volume,
                                           bool key_pressed) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  if (!audio_processing_)
    return 0;

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessData");
  DCHECK_EQ(audio_processing_->sample_rate_hz(),
            capture_converter_->sink_parameters().sample_rate());
  DCHECK_EQ(audio_processing_->num_input_channels(),
            capture_converter_->sink_parameters().channels());
  DCHECK_EQ(audio_processing_->num_output_channels(),
            capture_converter_->sink_parameters().channels());

  base::subtle::Atomic32 render_delay_ms =
      base::subtle::Acquire_Load(&render_delay_ms_);
  int64 capture_delay_ms = capture_delay.InMilliseconds();
  DCHECK_LT(capture_delay_ms,
            std::numeric_limits<base::subtle::Atomic32>::max());
  int total_delay_ms =  capture_delay_ms + render_delay_ms;
  if (total_delay_ms > 300) {
    LOG(WARNING) << "Large audio delay, capture delay: " << capture_delay_ms
                 << "ms; render delay: " << render_delay_ms << "ms";
  }

  audio_processing_->set_stream_delay_ms(total_delay_ms);

  DCHECK_LE(volume, WebRtcAudioDeviceImpl::kMaxVolumeLevel);
  webrtc::GainControl* agc = audio_processing_->gain_control();
  int err = agc->set_stream_analog_level(volume);
  DCHECK_EQ(err, 0) << "set_stream_analog_level() error: " << err;

  audio_processing_->set_stream_key_pressed(key_pressed);

  err = audio_processing_->ProcessStream(audio_frame);
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;

  if (typing_detector_ &&
      audio_frame->vad_activity_ != webrtc::AudioFrame::kVadUnknown) {
    bool vad_active =
        (audio_frame->vad_activity_ == webrtc::AudioFrame::kVadActive);
    bool typing_detected = typing_detector_->Process(key_pressed, vad_active);
    base::subtle::Release_Store(&typing_detected_, typing_detected);
  }

  // Return 0 if the volume has not been changed, otherwise return the new
  // volume.
  return (agc->stream_analog_level() == volume) ?
      0 : agc->stream_analog_level();
}

void MediaStreamAudioProcessor::StopAudioProcessing() {
  if (!audio_processing_.get())
    return;

  StopAecDump();

  if (playout_data_source_)
    playout_data_source_->RemovePlayoutSink(this);

  audio_processing_.reset();
}

bool MediaStreamAudioProcessor::IsAudioTrackProcessingEnabled() const {
  const std::string group_name =
      base::FieldTrialList::FindFullName("MediaStreamAudioTrackProcessing");
  return group_name == "Enabled" || CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAudioTrackProcessing);
}

}  // namespace content
