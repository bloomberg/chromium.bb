// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_rtp_stream.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/video_encode_accelerator.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_fifo.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_sender.h"
#include "media/cast/net/cast_transport_config.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "ui/gfx/geometry/size.h"

using media::cast::AudioSenderConfig;
using media::cast::VideoSenderConfig;

namespace {

const char kCodecNameOpus[] = "OPUS";
const char kCodecNameVp8[] = "VP8";
const char kCodecNameH264[] = "H264";

// To convert from kilobits per second to bits to per second.
const int kBitrateMultiplier = 1000;

CastRtpPayloadParams DefaultOpusPayload() {
  CastRtpPayloadParams payload;
  payload.payload_type = 127;
  payload.max_latency_ms = media::cast::kDefaultRtpMaxDelayMs;
  payload.ssrc = 1;
  payload.feedback_ssrc = 2;
  payload.clock_rate = media::cast::kDefaultAudioSamplingRate;
  // The value is 0 which means VBR.
  payload.min_bitrate = payload.max_bitrate =
      media::cast::kDefaultAudioEncoderBitrate;
  payload.channels = 2;
  payload.max_frame_rate = 100;  // 10 ms audio frames
  payload.codec_name = kCodecNameOpus;
  return payload;
}

CastRtpPayloadParams DefaultVp8Payload() {
  CastRtpPayloadParams payload;
  payload.payload_type = 96;
  payload.max_latency_ms = media::cast::kDefaultRtpMaxDelayMs;
  payload.ssrc = 11;
  payload.feedback_ssrc = 12;
  payload.clock_rate = media::cast::kVideoFrequency;
  payload.max_bitrate = 2000;
  payload.min_bitrate = 50;
  payload.channels = 1;
  payload.max_frame_rate = media::cast::kDefaultMaxFrameRate;
  payload.width = 1280;
  payload.height = 720;
  payload.codec_name = kCodecNameVp8;
  return payload;
}

CastRtpPayloadParams DefaultH264Payload() {
  CastRtpPayloadParams payload;
  // TODO(hshi): set different ssrc/rtpPayloadType values for H264 and VP8
  // once b/13696137 is fixed.
  payload.payload_type = 96;
  payload.max_latency_ms = media::cast::kDefaultRtpMaxDelayMs;
  payload.ssrc = 11;
  payload.feedback_ssrc = 12;
  payload.clock_rate = media::cast::kVideoFrequency;
  payload.max_bitrate = 2000;
  payload.min_bitrate = 50;
  payload.channels = 1;
  payload.max_frame_rate = media::cast::kDefaultMaxFrameRate;
  payload.width = 1280;
  payload.height = 720;
  payload.codec_name = kCodecNameH264;
  return payload;
}

bool IsHardwareVP8EncodingSupported() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableCastStreamingHWEncoding)) {
    DVLOG(1) << "Disabled hardware VP8 support for Cast Streaming.";
    return false;
  }

  // Query for hardware VP8 encoder support.
  std::vector<media::VideoEncodeAccelerator::SupportedProfile> vea_profiles =
      content::GetSupportedVideoEncodeAcceleratorProfiles();
  for (size_t i = 0; i < vea_profiles.size(); ++i) {
    if (vea_profiles[i].profile >= media::VP8PROFILE_MIN &&
        vea_profiles[i].profile <= media::VP8PROFILE_MAX) {
      return true;
    }
  }
  return false;
}

bool IsHardwareH264EncodingSupported() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableCastStreamingHWEncoding)) {
    DVLOG(1) << "Disabled hardware h264 support for Cast Streaming.";
    return false;
  }

  // Query for hardware H.264 encoder support.
  std::vector<media::VideoEncodeAccelerator::SupportedProfile> vea_profiles =
      content::GetSupportedVideoEncodeAcceleratorProfiles();
  for (size_t i = 0; i < vea_profiles.size(); ++i) {
    if (vea_profiles[i].profile >= media::H264PROFILE_MIN &&
        vea_profiles[i].profile <= media::H264PROFILE_MAX) {
      return true;
    }
  }
  return false;
}

int NumberOfEncodeThreads() {
  // We want to give CPU cycles for capturing and not to saturate the system
  // just for encoding. So on a lower end system with only 1 or 2 cores we
  // use only one thread for encoding.
  if (base::SysInfo::NumberOfProcessors() <= 2)
    return 1;

  // On higher end we want to use 2 threads for encoding to reduce latency.
  // In theory a physical CPU core has maximum 2 hyperthreads. Having 3 or
  // more logical processors means the system has at least 2 physical cores.
  return 2;
}

std::vector<CastRtpParams> SupportedAudioParams() {
  // TODO(hclam): Fill in more codecs here.
  std::vector<CastRtpParams> supported_params;
  supported_params.push_back(CastRtpParams(DefaultOpusPayload()));
  return supported_params;
}

std::vector<CastRtpParams> SupportedVideoParams() {
  std::vector<CastRtpParams> supported_params;
  if (IsHardwareH264EncodingSupported())
    supported_params.push_back(CastRtpParams(DefaultH264Payload()));
  supported_params.push_back(CastRtpParams(DefaultVp8Payload()));
  return supported_params;
}

bool ToAudioSenderConfig(const CastRtpParams& params,
                         AudioSenderConfig* config) {
  config->ssrc = params.payload.ssrc;
  config->receiver_ssrc = params.payload.feedback_ssrc;
  if (config->ssrc == config->receiver_ssrc)
    return false;
  config->min_playout_delay =
      base::TimeDelta::FromMilliseconds(
          params.payload.min_latency_ms ?
          params.payload.min_latency_ms :
          params.payload.max_latency_ms);
  config->max_playout_delay =
      base::TimeDelta::FromMilliseconds(params.payload.max_latency_ms);
  if (config->min_playout_delay <= base::TimeDelta())
    return false;
  if (config->min_playout_delay > config->max_playout_delay)
    return false;
  config->rtp_payload_type = params.payload.payload_type;
  config->use_external_encoder = false;
  config->frequency = params.payload.clock_rate;
  if (config->frequency < 8000)
    return false;
  config->channels = params.payload.channels;
  if (config->channels < 1)
    return false;
  config->bitrate = params.payload.max_bitrate * kBitrateMultiplier;
  if (params.payload.codec_name == kCodecNameOpus)
    config->codec = media::cast::CODEC_AUDIO_OPUS;
  else
    return false;
  config->aes_key = params.payload.aes_key;
  config->aes_iv_mask = params.payload.aes_iv_mask;
  return true;
}

bool ToVideoSenderConfig(const CastRtpParams& params,
                         VideoSenderConfig* config) {
  config->ssrc = params.payload.ssrc;
  config->receiver_ssrc = params.payload.feedback_ssrc;
  if (config->ssrc == config->receiver_ssrc)
    return false;
  config->min_playout_delay =
      base::TimeDelta::FromMilliseconds(
          params.payload.min_latency_ms ?
          params.payload.min_latency_ms :
          params.payload.max_latency_ms);
  config->max_playout_delay =
      base::TimeDelta::FromMilliseconds(params.payload.max_latency_ms);
  if (config->min_playout_delay <= base::TimeDelta())
    return false;
  if (config->min_playout_delay > config->max_playout_delay)
    return false;
  config->rtp_payload_type = params.payload.payload_type;
  config->min_bitrate = config->start_bitrate =
      params.payload.min_bitrate * kBitrateMultiplier;
  config->max_bitrate = params.payload.max_bitrate * kBitrateMultiplier;
  if (config->min_bitrate > config->max_bitrate)
    return false;
  config->start_bitrate = config->min_bitrate;
  config->max_frame_rate = static_cast<int>(
      std::max(1.0, params.payload.max_frame_rate) + 0.5);
  if (config->max_frame_rate > 120)
    return false;
  if (params.payload.codec_name == kCodecNameVp8) {
    config->use_external_encoder = IsHardwareVP8EncodingSupported();
    config->codec = media::cast::CODEC_VIDEO_VP8;
  } else if (params.payload.codec_name == kCodecNameH264) {
    config->use_external_encoder = IsHardwareH264EncodingSupported();
    config->codec = media::cast::CODEC_VIDEO_H264;
  } else {
    return false;
  }
  if (!config->use_external_encoder) {
    config->number_of_encode_threads = NumberOfEncodeThreads();
  }
  config->aes_key = params.payload.aes_key;
  config->aes_iv_mask = params.payload.aes_iv_mask;
  return true;
}

}  // namespace

// This class receives MediaStreamTrack events and video frames from a
// MediaStreamTrack.
//
// Threading: Video frames are received on the IO thread and then
// forwarded to media::cast::VideoFrameInput through a static method.
// Member variables of this class are only accessed on the render thread.
class CastVideoSink : public base::SupportsWeakPtr<CastVideoSink>,
                      public content::MediaStreamVideoSink {
 public:
  // |track| provides data for this sink.
  // |error_callback| is called if video formats don't match.
  CastVideoSink(const blink::WebMediaStreamTrack& track,
                const CastRtpStream::ErrorCallback& error_callback)
      : track_(track),
        sink_added_(false),
        error_callback_(error_callback) {}

  ~CastVideoSink() override {
    if (sink_added_)
      RemoveFromVideoTrack(this, track_);
  }

  // This static method is used to forward video frames to |frame_input|.
  static void OnVideoFrame(
      // These parameters are already bound when callback is created.
      const CastRtpStream::ErrorCallback& error_callback,
      const scoped_refptr<media::cast::VideoFrameInput> frame_input,
      // These parameters are passed for each frame.
      const scoped_refptr<media::VideoFrame>& frame,
      const base::TimeTicks& estimated_capture_time) {
    base::TimeTicks timestamp;
    if (estimated_capture_time.is_null())
      timestamp = base::TimeTicks::Now();
    else
      timestamp = estimated_capture_time;

    // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
    TRACE_EVENT_INSTANT2(
        "cast_perf_test", "MediaStreamVideoSink::OnVideoFrame",
        TRACE_EVENT_SCOPE_THREAD,
        "timestamp",  timestamp.ToInternalValue(),
        "time_delta", frame->timestamp().ToInternalValue());
    frame_input->InsertRawVideoFrame(frame, timestamp);
  }

  // Attach this sink to a video track represented by |track_|.
  // Data received from the track will be submitted to |frame_input|.
  void AddToTrack(
      const scoped_refptr<media::cast::VideoFrameInput>& frame_input) {
    DCHECK(!sink_added_);
    sink_added_ = true;
    AddToVideoTrack(
        this,
        base::Bind(
            &CastVideoSink::OnVideoFrame,
            error_callback_,
            frame_input),
        track_);
  }

 private:
  blink::WebMediaStreamTrack track_;
  bool sink_added_;
  CastRtpStream::ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(CastVideoSink);
};

// Receives audio data from a MediaStreamTrack. Data is submitted to
// media::cast::FrameInput.
//
// Threading: Audio frames are received on the real-time audio thread.
// Note that RemoveFromAudioTrack() is synchronous and we have
// gurantee that there will be no more audio data after calling it.
class CastAudioSink : public base::SupportsWeakPtr<CastAudioSink>,
                      public content::MediaStreamAudioSink,
                      public media::AudioConverter::InputCallback {
 public:
  // |track| provides data for this sink.
  CastAudioSink(const blink::WebMediaStreamTrack& track,
                int output_channels,
                int output_sample_rate)
      : track_(track),
        output_channels_(output_channels),
        output_sample_rate_(output_sample_rate),
        current_input_bus_(nullptr),
        sample_frames_in_(0),
        sample_frames_out_(0) {}

  ~CastAudioSink() override {
    if (frame_input_.get())
      RemoveFromAudioTrack(this, track_);
  }

  // Add this sink to the track. Data received from the track will be
  // submitted to |frame_input|.
  void AddToTrack(
      const scoped_refptr<media::cast::AudioFrameInput>& frame_input) {
    DCHECK(frame_input.get());
    DCHECK(!frame_input_.get());
    // This member is written here and then accessed on the IO thread
    // We will not get data until AddToAudioTrack is called so it is
    // safe to access this member now.
    frame_input_ = frame_input;
    AddToAudioTrack(this, track_);
  }

 protected:
  // Called on real-time audio thread.
  void OnData(const media::AudioBus& input_bus,
              base::TimeTicks estimated_capture_time) override {
    DCHECK(input_params_.IsValid());
    DCHECK_EQ(input_bus.channels(), input_params_.channels());
    DCHECK_EQ(input_bus.frames(), input_params_.frames_per_buffer());
    DCHECK(!estimated_capture_time.is_null());
    DCHECK(converter_.get());

    // Determine the duration of the audio signal enqueued within |converter_|.
    const base::TimeDelta signal_duration_already_buffered =
        (sample_frames_in_ * base::TimeDelta::FromSeconds(1) /
             input_params_.sample_rate()) -
        (sample_frames_out_ * base::TimeDelta::FromSeconds(1) /
             output_sample_rate_);
    DVLOG(2) << "Audio reference time adjustment: -("
             << signal_duration_already_buffered.InMicroseconds() << " us)";
    const base::TimeTicks capture_time_of_first_converted_sample =
        estimated_capture_time - signal_duration_already_buffered;

    // Convert the entire input signal.  AudioConverter is efficient in that no
    // additional copying or conversion will occur if the input signal is in the
    // same format as the output.  Note that, while the number of sample frames
    // provided as input is always the same, the chunk size (and the size of the
    // |audio_bus| here) can be variable.  This is not an issue since
    // media::cast::AudioFrameInput can handle variable-sized AudioBuses.
    scoped_ptr<media::AudioBus> audio_bus =
        media::AudioBus::Create(output_channels_, converter_->ChunkSize());
    // AudioConverter will call ProvideInput() to fetch from |current_data_|.
    current_input_bus_ = &input_bus;
    converter_->Convert(audio_bus.get());
    DCHECK(!current_input_bus_);  // ProvideInput() called exactly once?

    sample_frames_in_ += input_params_.frames_per_buffer();
    sample_frames_out_ += audio_bus->frames();

    frame_input_->InsertAudio(audio_bus.Pass(),
                              capture_time_of_first_converted_sample);
  }

  // Called on real-time audio thread.
  void OnSetFormat(const media::AudioParameters& params) override {
    if (input_params_.Equals(params))
      return;
    input_params_ = params;

    DVLOG(1) << "Setting up audio resampling: {"
             << input_params_.channels() << " channels, "
             << input_params_.sample_rate() << " Hz} --> {"
             << output_channels_ << " channels, "
             << output_sample_rate_ << " Hz}";
    const media::AudioParameters output_params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::GuessChannelLayout(output_channels_),
        output_sample_rate_, 32,
        output_sample_rate_ * input_params_.frames_per_buffer() /
            input_params_.sample_rate());
    converter_.reset(
        new media::AudioConverter(input_params_, output_params, false));
    converter_->AddInput(this);
    sample_frames_in_ = 0;
    sample_frames_out_ = 0;
  }

  // Called on real-time audio thread.
  double ProvideInput(media::AudioBus* audio_bus,
                      base::TimeDelta buffer_delay) override {
    DCHECK(current_input_bus_);
    current_input_bus_->CopyTo(audio_bus);
    current_input_bus_ = nullptr;
    return 1.0;
  }

 private:
  const blink::WebMediaStreamTrack track_;
  const int output_channels_;
  const int output_sample_rate_;

  // This must be set before the real-time audio thread starts calling OnData(),
  // and remain unchanged until after the thread will stop calling OnData().
  scoped_refptr<media::cast::AudioFrameInput> frame_input_;

  // These members are accessed on the real-time audio time only.
  media::AudioParameters input_params_;
  scoped_ptr<media::AudioConverter> converter_;
  const media::AudioBus* current_input_bus_;
  int64 sample_frames_in_;
  int64 sample_frames_out_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioSink);
};

CastRtpParams::CastRtpParams(const CastRtpPayloadParams& payload_params)
    : payload(payload_params) {}

CastCodecSpecificParams::CastCodecSpecificParams() {}

CastCodecSpecificParams::~CastCodecSpecificParams() {}

CastRtpPayloadParams::CastRtpPayloadParams()
    : payload_type(0),
      max_latency_ms(0),
      min_latency_ms(0),
      ssrc(0),
      feedback_ssrc(0),
      clock_rate(0),
      max_bitrate(0),
      min_bitrate(0),
      channels(0),
      max_frame_rate(0.0),
      width(0),
      height(0) {}

CastRtpPayloadParams::~CastRtpPayloadParams() {}

CastRtpParams::CastRtpParams() {}

CastRtpParams::~CastRtpParams() {}

CastRtpStream::CastRtpStream(const blink::WebMediaStreamTrack& track,
                             const scoped_refptr<CastSession>& session)
    : track_(track), cast_session_(session), weak_factory_(this) {}

CastRtpStream::~CastRtpStream() {}

std::vector<CastRtpParams> CastRtpStream::GetSupportedParams() {
  if (IsAudio())
    return SupportedAudioParams();
  else
    return SupportedVideoParams();
}

CastRtpParams CastRtpStream::GetParams() { return params_; }

void CastRtpStream::Start(const CastRtpParams& params,
                          const base::Closure& start_callback,
                          const base::Closure& stop_callback,
                          const ErrorCallback& error_callback) {
  DVLOG(1) << "CastRtpStream::Start = " << (IsAudio() ? "audio" : "video");
  stop_callback_ = stop_callback;
  error_callback_ = error_callback;

  if (IsAudio()) {
    AudioSenderConfig config;
    if (!ToAudioSenderConfig(params, &config)) {
      DidEncounterError("Invalid parameters for audio.");
      return;
    }

    // In case of error we have to go through DidEncounterError() to stop
    // the streaming after reporting the error.
    audio_sink_.reset(new CastAudioSink(
        track_,
        params.payload.channels,
        params.payload.clock_rate));
    cast_session_->StartAudio(
        config,
        base::Bind(&CastAudioSink::AddToTrack, audio_sink_->AsWeakPtr()),
        base::Bind(&CastRtpStream::DidEncounterError,
                   weak_factory_.GetWeakPtr()));
    start_callback.Run();
  } else {
    VideoSenderConfig config;
    if (!ToVideoSenderConfig(params, &config)) {
      DidEncounterError("Invalid parameters for video.");
      return;
    }
    // See the code for audio above for explanation of callbacks.
    video_sink_.reset(new CastVideoSink(
        track_,
        media::BindToCurrentLoop(base::Bind(&CastRtpStream::DidEncounterError,
                                            weak_factory_.GetWeakPtr()))));
    cast_session_->StartVideo(
        config,
        base::Bind(&CastVideoSink::AddToTrack, video_sink_->AsWeakPtr()),
        base::Bind(&CastRtpStream::DidEncounterError,
                   weak_factory_.GetWeakPtr()));
    start_callback.Run();
  }
}

void CastRtpStream::Stop() {
  DVLOG(1) << "CastRtpStream::Stop = " << (IsAudio() ? "audio" : "video");
  audio_sink_.reset();
  video_sink_.reset();
  if (!stop_callback_.is_null())
    stop_callback_.Run();
}

void CastRtpStream::ToggleLogging(bool enable) {
  DVLOG(1) << "CastRtpStream::ToggleLogging(" << enable << ") = "
           << (IsAudio() ? "audio" : "video");
  cast_session_->ToggleLogging(IsAudio(), enable);
}

void CastRtpStream::GetRawEvents(
    const base::Callback<void(scoped_ptr<base::BinaryValue>)>& callback,
    const std::string& extra_data) {
  DVLOG(1) << "CastRtpStream::GetRawEvents = "
           << (IsAudio() ? "audio" : "video");
  cast_session_->GetEventLogsAndReset(IsAudio(), extra_data, callback);
}

void CastRtpStream::GetStats(
    const base::Callback<void(scoped_ptr<base::DictionaryValue>)>& callback) {
  DVLOG(1) << "CastRtpStream::GetStats = "
           << (IsAudio() ? "audio" : "video");
  cast_session_->GetStatsAndReset(IsAudio(), callback);
}

bool CastRtpStream::IsAudio() const {
  return track_.source().type() == blink::WebMediaStreamSource::TypeAudio;
}

void CastRtpStream::DidEncounterError(const std::string& message) {
  DVLOG(1) << "CastRtpStream::DidEncounterError(" << message << ") = "
           << (IsAudio() ? "audio" : "video");
  // Save the WeakPtr first because the error callback might delete this object.
  base::WeakPtr<CastRtpStream> ptr = weak_factory_.GetWeakPtr();
  error_callback_.Run(message);
  content::RenderThread::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&CastRtpStream::Stop, ptr));
}
