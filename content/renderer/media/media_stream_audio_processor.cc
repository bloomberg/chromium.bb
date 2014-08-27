// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_processor.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#if defined(OS_MACOSX)
#include "base/metrics/field_trial.h"
#endif
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

#if defined(OS_ANDROID)
const int kAudioProcessingSampleRate = 16000;
#else
const int kAudioProcessingSampleRate = 32000;
#endif
const int kAudioProcessingNumberOfChannels = 1;

AudioProcessing::ChannelLayout MapLayout(media::ChannelLayout media_layout) {
  switch (media_layout) {
    case media::CHANNEL_LAYOUT_MONO:
      return AudioProcessing::kMono;
    case media::CHANNEL_LAYOUT_STEREO:
      return AudioProcessing::kStereo;
    case media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC:
      return AudioProcessing::kStereoAndKeyboard;
    default:
      NOTREACHED() << "Layout not supported: " << media_layout;
      return AudioProcessing::kMono;
  }
}

AudioProcessing::ChannelLayout ChannelsToLayout(int num_channels) {
  switch (num_channels) {
    case 1:
      return AudioProcessing::kMono;
    case 2:
      return AudioProcessing::kStereo;
    default:
      NOTREACHED() << "Channels not supported: " << num_channels;
      return AudioProcessing::kMono;
  }
}

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

// Wraps AudioBus to provide access to the array of channel pointers, since this
// is the type webrtc::AudioProcessing deals in. The array is refreshed on every
// channel_ptrs() call, and will be valid until the underlying AudioBus pointers
// are changed, e.g. through calls to SetChannelData() or SwapChannels().
//
// All methods are called on one of the capture or render audio threads
// exclusively.
class MediaStreamAudioBus {
 public:
  MediaStreamAudioBus(int channels, int frames)
      : bus_(media::AudioBus::Create(channels, frames)),
        channel_ptrs_(new float*[channels]) {
    // May be created in the main render thread and used in the audio threads.
    thread_checker_.DetachFromThread();
  }

  media::AudioBus* bus() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return bus_.get();
  }

  float* const* channel_ptrs() {
    DCHECK(thread_checker_.CalledOnValidThread());
    for (int i = 0; i < bus_->channels(); ++i) {
      channel_ptrs_[i] = bus_->channel(i);
    }
    return channel_ptrs_.get();
  }

 private:
  base::ThreadChecker thread_checker_;
  scoped_ptr<media::AudioBus> bus_;
  scoped_ptr<float*[]> channel_ptrs_;
};

// Wraps AudioFifo to provide a cleaner interface to MediaStreamAudioProcessor.
// It avoids the FIFO when the source and destination frames match. All methods
// are called on one of the capture or render audio threads exclusively.
class MediaStreamAudioFifo {
 public:
  MediaStreamAudioFifo(int channels, int source_frames,
                       int destination_frames)
     : source_frames_(source_frames),
       destination_(new MediaStreamAudioBus(channels, destination_frames)),
       data_available_(false) {
    if (source_frames != destination_frames) {
      // Since we require every Push to be followed by as many Consumes as
      // possible, twice the larger of the two is a (probably) loose upper bound
      // on the FIFO size.
      const int fifo_frames = 2 * std::max(source_frames, destination_frames);
      fifo_.reset(new media::AudioFifo(channels, fifo_frames));
    }

    // May be created in the main render thread and used in the audio threads.
    thread_checker_.DetachFromThread();
  }

  void Push(const media::AudioBus* source) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_EQ(source->channels(), destination_->bus()->channels());
    DCHECK_EQ(source->frames(), source_frames_);

    if (fifo_) {
      fifo_->Push(source);
    } else {
      source->CopyTo(destination_->bus());
      data_available_ = true;
    }
  }

  // Returns true if there are destination_frames() of data available to be
  // consumed, and otherwise false.
  bool Consume(MediaStreamAudioBus** destination) {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (fifo_) {
      if (fifo_->frames() < destination_->bus()->frames())
        return false;

      fifo_->Consume(destination_->bus(), 0, destination_->bus()->frames());
    } else {
      if (!data_available_)
        return false;

      // The data was already copied to |destination_| in this case.
      data_available_ = false;
    }

    *destination = destination_.get();
    return true;
  }

 private:
  base::ThreadChecker thread_checker_;
  const int source_frames_;  // For a DCHECK.
  scoped_ptr<MediaStreamAudioBus> destination_;
  scoped_ptr<media::AudioFifo> fifo_;
  // Only used when the FIFO is disabled;
  bool data_available_;
};

bool MediaStreamAudioProcessor::IsAudioTrackProcessingEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableAudioTrackProcessing);
}

MediaStreamAudioProcessor::MediaStreamAudioProcessor(
    const blink::WebMediaConstraints& constraints,
    int effects,
    WebRtcPlayoutDataSource* playout_data_source)
    : render_delay_ms_(0),
      playout_data_source_(playout_data_source),
      audio_mirroring_(false),
      typing_detected_(false),
      stopped_(false) {
  capture_thread_checker_.DetachFromThread();
  render_thread_checker_.DetachFromThread();
  InitializeAudioProcessingModule(constraints, effects);
  if (IsAudioTrackProcessingEnabled()) {
    aec_dump_message_filter_ = AecDumpMessageFilter::Get();
    // In unit tests not creating a message filter, |aec_dump_message_filter_|
    // will be NULL. We can just ignore that. Other unit tests and browser tests
    // ensure that we do get the filter when we should.
    if (aec_dump_message_filter_.get())
      aec_dump_message_filter_->AddDelegate(this);
  }
}

MediaStreamAudioProcessor::~MediaStreamAudioProcessor() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  Stop();
}

void MediaStreamAudioProcessor::OnCaptureFormatChanged(
    const media::AudioParameters& input_format) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more PushCaptureData() and ProcessAndConsumeData() callbacks
  // on the capture thread.
  InitializeCaptureFifo(input_format);

  // Reset the |capture_thread_checker_| since the capture data will come from
  // a new capture thread.
  capture_thread_checker_.DetachFromThread();
}

void MediaStreamAudioProcessor::PushCaptureData(
    const media::AudioBus* audio_source) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  capture_fifo_->Push(audio_source);
}

bool MediaStreamAudioProcessor::ProcessAndConsumeData(
    base::TimeDelta capture_delay, int volume, bool key_pressed,
    int* new_volume, int16** out) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessAndConsumeData");

  MediaStreamAudioBus* process_bus;
  if (!capture_fifo_->Consume(&process_bus))
    return false;

  // Use the process bus directly if audio processing is disabled.
  MediaStreamAudioBus* output_bus = process_bus;
  *new_volume = 0;
  if (audio_processing_) {
    output_bus = output_bus_.get();
    *new_volume = ProcessData(process_bus->channel_ptrs(),
                              process_bus->bus()->frames(), capture_delay,
                              volume, key_pressed, output_bus->channel_ptrs());
  }

  // Swap channels before interleaving the data.
  if (audio_mirroring_ &&
      output_format_.channel_layout() == media::CHANNEL_LAYOUT_STEREO) {
    // Swap the first and second channels.
    output_bus->bus()->SwapChannels(0, 1);
  }

  output_bus->bus()->ToInterleaved(output_bus->bus()->frames(),
                                   sizeof(int16),
                                   output_data_.get());
  *out = output_data_.get();

  return true;
}

void MediaStreamAudioProcessor::Stop() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (stopped_)
    return;

  stopped_ = true;

  if (aec_dump_message_filter_.get()) {
    aec_dump_message_filter_->RemoveDelegate(this);
    aec_dump_message_filter_ = NULL;
  }

  if (!audio_processing_.get())
    return;

  StopEchoCancellationDump(audio_processing_.get());

  if (playout_data_source_) {
    playout_data_source_->RemovePlayoutSink(this);
    playout_data_source_ = NULL;
  }
}

const media::AudioParameters& MediaStreamAudioProcessor::InputFormat() const {
  return input_format_;
}

const media::AudioParameters& MediaStreamAudioProcessor::OutputFormat() const {
  return output_format_;
}

void MediaStreamAudioProcessor::OnAecDumpFile(
    const IPC::PlatformFileForTransit& file_handle) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::File file = IPC::PlatformFileForTransitToFile(file_handle);
  DCHECK(file.IsValid());

  if (audio_processing_)
    StartEchoCancellationDump(audio_processing_.get(), file.Pass());
  else
    file.Close();
}

void MediaStreamAudioProcessor::OnDisableAecDump() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (audio_processing_)
    StopEchoCancellationDump(audio_processing_.get());
}

void MediaStreamAudioProcessor::OnIpcClosing() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  aec_dump_message_filter_ = NULL;
}

void MediaStreamAudioProcessor::OnPlayoutData(media::AudioBus* audio_bus,
                                              int sample_rate,
                                              int audio_delay_milliseconds) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  DCHECK(audio_processing_->echo_control_mobile()->is_enabled() ^
         audio_processing_->echo_cancellation()->is_enabled());

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::OnPlayoutData");
  DCHECK_LT(audio_delay_milliseconds,
            std::numeric_limits<base::subtle::Atomic32>::max());
  base::subtle::Release_Store(&render_delay_ms_, audio_delay_milliseconds);

  InitializeRenderFifoIfNeeded(sample_rate, audio_bus->channels(),
                               audio_bus->frames());

  render_fifo_->Push(audio_bus);
  MediaStreamAudioBus* analysis_bus;
  while (render_fifo_->Consume(&analysis_bus)) {
    audio_processing_->AnalyzeReverseStream(
        analysis_bus->channel_ptrs(),
        analysis_bus->bus()->frames(),
        sample_rate,
        ChannelsToLayout(audio_bus->channels()));
  }
}

void MediaStreamAudioProcessor::OnPlayoutDataSourceChanged() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more OnPlayoutData() callback on the render thread.
  render_thread_checker_.DetachFromThread();
  render_fifo_.reset();
}

void MediaStreamAudioProcessor::GetStats(AudioProcessorStats* stats) {
  stats->typing_noise_detected =
      (base::subtle::Acquire_Load(&typing_detected_) != false);
  GetAecStats(audio_processing_.get(), stats);
}

void MediaStreamAudioProcessor::InitializeAudioProcessingModule(
    const blink::WebMediaConstraints& constraints, int effects) {
  DCHECK(!audio_processing_);

  MediaAudioConstraints audio_constraints(constraints, effects);

  // Audio mirroring can be enabled even though audio processing is otherwise
  // disabled.
  audio_mirroring_ = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogAudioMirroring);

  if (!IsAudioTrackProcessingEnabled()) {
    RecordProcessingState(AUDIO_PROCESSING_IN_WEBRTC);
    return;
  }

#if defined(OS_IOS)
  // On iOS, VPIO provides built-in AGC and AEC.
  const bool echo_cancellation = false;
  const bool goog_agc = false;
#else
  const bool echo_cancellation =
      audio_constraints.GetEchoCancellationProperty();
  const bool goog_agc = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogAutoGainControl);
#endif

#if defined(OS_IOS) || defined(OS_ANDROID)
  const bool goog_experimental_aec = false;
  const bool goog_typing_detection = false;
#else
  const bool goog_experimental_aec = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogExperimentalEchoCancellation);
  const bool goog_typing_detection = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogTypingNoiseDetection);
#endif

  const bool goog_ns = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogNoiseSuppression);
  const bool goog_experimental_ns = audio_constraints.GetProperty(
      MediaAudioConstraints::kGoogExperimentalNoiseSuppression);
 const bool goog_high_pass_filter = audio_constraints.GetProperty(
     MediaAudioConstraints::kGoogHighpassFilter);

  // Return immediately if no goog constraint is enabled.
  if (!echo_cancellation && !goog_experimental_aec && !goog_ns &&
      !goog_high_pass_filter && !goog_typing_detection &&
      !goog_agc && !goog_experimental_ns) {
    RecordProcessingState(AUDIO_PROCESSING_DISABLED);
    return;
  }

  // Experimental options provided at creation.
  webrtc::Config config;
  if (goog_experimental_aec)
    config.Set<webrtc::DelayCorrection>(new webrtc::DelayCorrection(true));
  if (goog_experimental_ns)
    config.Set<webrtc::ExperimentalNs>(new webrtc::ExperimentalNs(true));
#if defined(OS_MACOSX)
  if (base::FieldTrialList::FindFullName("NoReportedDelayOnMac") == "Enabled")
    config.Set<webrtc::ReportedDelay>(new webrtc::ReportedDelay(false));
#endif

  // Create and configure the webrtc::AudioProcessing.
  audio_processing_.reset(webrtc::AudioProcessing::Create(config));

  // Enable the audio processing components.
  if (echo_cancellation) {
    EnableEchoCancellation(audio_processing_.get());

    if (playout_data_source_)
      playout_data_source_->AddPlayoutSink(this);
  }

  if (goog_ns)
    EnableNoiseSuppression(audio_processing_.get());

  if (goog_high_pass_filter)
    EnableHighPassFilter(audio_processing_.get());

  if (goog_typing_detection) {
    // TODO(xians): Remove this |typing_detector_| after the typing suppression
    // is enabled by default.
    typing_detector_.reset(new webrtc::TypingDetection());
    EnableTypingDetection(audio_processing_.get(), typing_detector_.get());
  }

  if (goog_agc)
    EnableAutomaticGainControl(audio_processing_.get());

  RecordProcessingState(AUDIO_PROCESSING_ENABLED);
}

void MediaStreamAudioProcessor::InitializeCaptureFifo(
    const media::AudioParameters& input_format) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(input_format.IsValid());
  input_format_ = input_format;

  // TODO(ajm): For now, we assume fixed parameters for the output when audio
  // processing is enabled, to match the previous behavior. We should either
  // use the input parameters (in which case, audio processing will convert
  // at output) or ideally, have a backchannel from the sink to know what
  // format it would prefer.
  const int output_sample_rate = audio_processing_ ?
      kAudioProcessingSampleRate : input_format.sample_rate();
  const media::ChannelLayout output_channel_layout = audio_processing_ ?
      media::GuessChannelLayout(kAudioProcessingNumberOfChannels) :
      input_format.channel_layout();

  // webrtc::AudioProcessing requires a 10 ms chunk size. We use this native
  // size when processing is enabled. When disabled we use the same size as
  // the source if less than 10 ms.
  //
  // TODO(ajm): This conditional buffer size appears to be assuming knowledge of
  // the sink based on the source parameters. PeerConnection sinks seem to want
  // 10 ms chunks regardless, while WebAudio sinks want less, and we're assuming
  // we can identify WebAudio sinks by the input chunk size. Less fragile would
  // be to have the sink actually tell us how much it wants (as in the above
  // TODO).
  int processing_frames = input_format.sample_rate() / 100;
  int output_frames = output_sample_rate / 100;
  if (!audio_processing_ && input_format.frames_per_buffer() < output_frames) {
    processing_frames = input_format.frames_per_buffer();
    output_frames = processing_frames;
  }

  output_format_ = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      output_channel_layout,
      output_sample_rate,
      16,
      output_frames);

  capture_fifo_.reset(
      new MediaStreamAudioFifo(input_format.channels(),
                               input_format.frames_per_buffer(),
                               processing_frames));

  if (audio_processing_) {
    output_bus_.reset(new MediaStreamAudioBus(output_format_.channels(),
                                              output_frames));
  }
  output_data_.reset(new int16[output_format_.GetBytesPerBuffer() /
                               sizeof(int16)]);
}

void MediaStreamAudioProcessor::InitializeRenderFifoIfNeeded(
    int sample_rate, int number_of_channels, int frames_per_buffer) {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  if (render_fifo_.get() &&
      render_format_.sample_rate() == sample_rate &&
      render_format_.channels() == number_of_channels &&
      render_format_.frames_per_buffer() == frames_per_buffer) {
    // Do nothing if the |render_fifo_| has been setup properly.
    return;
  }

  render_format_ = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(number_of_channels),
      sample_rate,
      16,
      frames_per_buffer);

  const int analysis_frames = sample_rate / 100;  // 10 ms chunks.
  render_fifo_.reset(
      new MediaStreamAudioFifo(number_of_channels,
                               frames_per_buffer,
                               analysis_frames));
}

int MediaStreamAudioProcessor::ProcessData(const float* const* process_ptrs,
                                           int process_frames,
                                           base::TimeDelta capture_delay,
                                           int volume,
                                           bool key_pressed,
                                           float* const* output_ptrs) {
  DCHECK(audio_processing_);
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessData");

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

  webrtc::AudioProcessing* ap = audio_processing_.get();
  ap->set_stream_delay_ms(total_delay_ms);

  DCHECK_LE(volume, WebRtcAudioDeviceImpl::kMaxVolumeLevel);
  webrtc::GainControl* agc = ap->gain_control();
  int err = agc->set_stream_analog_level(volume);
  DCHECK_EQ(err, 0) << "set_stream_analog_level() error: " << err;

  ap->set_stream_key_pressed(key_pressed);

  err = ap->ProcessStream(process_ptrs,
                          process_frames,
                          input_format_.sample_rate(),
                          MapLayout(input_format_.channel_layout()),
                          output_format_.sample_rate(),
                          MapLayout(output_format_.channel_layout()),
                          output_ptrs);
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;

  if (typing_detector_) {
    webrtc::VoiceDetection* vad = ap->voice_detection();
    DCHECK(vad->is_enabled());
    bool detected = typing_detector_->Process(key_pressed,
                                              vad->stream_has_voice());
    base::subtle::Release_Store(&typing_detected_, detected);
  }

  // Return 0 if the volume hasn't been changed, and otherwise the new volume.
  return (agc->stream_analog_level() == volume) ?
      0 : agc->stream_analog_level();
}

}  // namespace content
