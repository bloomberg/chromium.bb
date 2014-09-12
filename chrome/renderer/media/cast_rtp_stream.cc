// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_rtp_stream.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/video_encode_accelerator.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_fifo.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/multi_channel_resampler.h"
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

// This constant defines the number of sets of audio data to buffer
// in the FIFO. If input audio and output data have different resampling
// rates then buffer is necessary to avoid audio glitches.
// See CastAudioSink::ResampleData() and CastAudioSink::OnSetFormat()
// for more defaults.
const int kBufferAudioData = 2;

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
  config->incoming_feedback_ssrc = params.payload.feedback_ssrc;
  if (config->ssrc == config->incoming_feedback_ssrc)
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
  config->incoming_feedback_ssrc = params.payload.feedback_ssrc;
  if (config->ssrc == config->incoming_feedback_ssrc)
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
  config->width = params.payload.width;
  config->height = params.payload.height;
  if (config->width < 2 || config->height < 2)
    return false;
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
  // |expected_natural_size| is the expected dimension of the video frame.
  // |error_callback| is called if video formats don't match.
  CastVideoSink(const blink::WebMediaStreamTrack& track,
                const gfx::Size& expected_natural_size,
                const CastRtpStream::ErrorCallback& error_callback)
      : track_(track),
        sink_added_(false),
        expected_natural_size_(expected_natural_size),
        error_callback_(error_callback) {}

  virtual ~CastVideoSink() {
    if (sink_added_)
      RemoveFromVideoTrack(this, track_);
  }

  // This static method is used to forward video frames to |frame_input|.
  static void OnVideoFrame(
      // These parameters are already bound when callback is created.
      const gfx::Size& expected_natural_size,
      const CastRtpStream::ErrorCallback& error_callback,
      const scoped_refptr<media::cast::VideoFrameInput> frame_input,
      // These parameters are passed for each frame.
      const scoped_refptr<media::VideoFrame>& frame,
      const media::VideoCaptureFormat& format,
      const base::TimeTicks& estimated_capture_time) {
    if (frame->natural_size() != expected_natural_size) {
      error_callback.Run(
          base::StringPrintf("Video frame resolution does not match config."
                             " Expected %dx%d. Got %dx%d.",
                             expected_natural_size.width(),
                             expected_natural_size.height(),
                             frame->natural_size().width(),
                             frame->natural_size().height()));
      return;
    }

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
            expected_natural_size_,
            error_callback_,
            frame_input),
        track_);
  }

 private:
  blink::WebMediaStreamTrack track_;
  bool sink_added_;
  gfx::Size expected_natural_size_;
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
                      public content::MediaStreamAudioSink {
 public:
  // |track| provides data for this sink.
  // |error_callback| is called if audio formats don't match.
  CastAudioSink(const blink::WebMediaStreamTrack& track,
                const CastRtpStream::ErrorCallback& error_callback,
                int output_channels,
                int output_sample_rate)
      : track_(track),
        sink_added_(false),
        error_callback_(error_callback),
        weak_factory_(this),
        output_channels_(output_channels),
        output_sample_rate_(output_sample_rate),
        input_preroll_(0) {}

  virtual ~CastAudioSink() {
    if (sink_added_)
      RemoveFromAudioTrack(this, track_);
  }

  // Called on real-time audio thread.
  // content::MediaStreamAudioSink implementation.
  virtual void OnData(const int16* audio_data,
                      int sample_rate,
                      int number_of_channels,
                      int number_of_frames) OVERRIDE {
    scoped_ptr<media::AudioBus> input_bus;
    if (resampler_) {
      input_bus = ResampleData(
          audio_data, sample_rate, number_of_channels, number_of_frames);
      if (!input_bus)
        return;
    } else {
      input_bus = media::AudioBus::Create(
          number_of_channels, number_of_frames);
      input_bus->FromInterleaved(
          audio_data, number_of_frames, number_of_channels);
    }

    // TODO(hclam): Pass in the accurate capture time to have good
    // audio / video sync.
    frame_input_->InsertAudio(input_bus.Pass(), base::TimeTicks::Now());
  }

  // Return a resampled audio data from input. This is called when the
  // input sample rate doesn't match the output.
  // The flow of data is as follows:
  // |audio_data| ->
  //     AudioFifo |fifo_| ->
  //         MultiChannelResampler |resampler|.
  //
  // The resampler pulls data out of the FIFO and resample the data in
  // frequency domain. It might call |fifo_| for more than once. But no more
  // than |kBufferAudioData| times. We preroll audio data into the FIFO to
  // make sure there's enough data for resampling.
  scoped_ptr<media::AudioBus> ResampleData(
      const int16* audio_data,
      int sample_rate,
      int number_of_channels,
      int number_of_frames) {
    DCHECK_EQ(number_of_channels, output_channels_);
    fifo_input_bus_->FromInterleaved(
        audio_data, number_of_frames, number_of_channels);
    fifo_->Push(fifo_input_bus_.get());

    if (input_preroll_ < kBufferAudioData - 1) {
      ++input_preroll_;
      return scoped_ptr<media::AudioBus>();
    }

    scoped_ptr<media::AudioBus> output_bus(
        media::AudioBus::Create(
            output_channels_,
            output_sample_rate_ * fifo_input_bus_->frames() / sample_rate));

    // Resampler will then call ProvideData() below to fetch data from
    // |input_data_|.
    resampler_->Resample(output_bus->frames(), output_bus.get());
    return output_bus.Pass();
  }

  // Called on real-time audio thread.
  virtual void OnSetFormat(const media::AudioParameters& params) OVERRIDE {
    if (params.sample_rate() == output_sample_rate_)
      return;
    fifo_.reset(new media::AudioFifo(
        output_channels_,
        kBufferAudioData * params.frames_per_buffer()));
    fifo_input_bus_ = media::AudioBus::Create(
        params.channels(), params.frames_per_buffer());
    resampler_.reset(new media::MultiChannelResampler(
        output_channels_,
        static_cast<double>(params.sample_rate()) / output_sample_rate_,
        params.frames_per_buffer(),
        base::Bind(&CastAudioSink::ProvideData, base::Unretained(this))));
  }

  // Add this sink to the track. Data received from the track will be
  // submitted to |frame_input|.
  void AddToTrack(
      const scoped_refptr<media::cast::AudioFrameInput>& frame_input) {
    DCHECK(!sink_added_);
    sink_added_ = true;

    // This member is written here and then accessed on the IO thread
    // We will not get data until AddToAudioTrack is called so it is
    // safe to access this member now.
    frame_input_ = frame_input;
    AddToAudioTrack(this, track_);
  }

  void ProvideData(int frame_delay, media::AudioBus* output_bus) {
    fifo_->Consume(output_bus, 0, output_bus->frames());
  }

 private:
  blink::WebMediaStreamTrack track_;
  bool sink_added_;
  CastRtpStream::ErrorCallback error_callback_;
  base::WeakPtrFactory<CastAudioSink> weak_factory_;

  const int output_channels_;
  const int output_sample_rate_;

  // These member are accessed on the real-time audio time only.
  scoped_refptr<media::cast::AudioFrameInput> frame_input_;
  scoped_ptr<media::MultiChannelResampler> resampler_;
  scoped_ptr<media::AudioFifo> fifo_;
  scoped_ptr<media::AudioBus> fifo_input_bus_;
  int input_preroll_;

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
  VLOG(1) << "CastRtpStream::Start = " << (IsAudio() ? "audio" : "video");
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
        media::BindToCurrentLoop(base::Bind(&CastRtpStream::DidEncounterError,
                                            weak_factory_.GetWeakPtr())),
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
        gfx::Size(config.width, config.height),
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
  VLOG(1) << "CastRtpStream::Stop = " << (IsAudio() ? "audio" : "video");
  audio_sink_.reset();
  video_sink_.reset();
  if (!stop_callback_.is_null())
    stop_callback_.Run();
}

void CastRtpStream::ToggleLogging(bool enable) {
  VLOG(1) << "CastRtpStream::ToggleLogging(" << enable << ") = "
          << (IsAudio() ? "audio" : "video");
  cast_session_->ToggleLogging(IsAudio(), enable);
}

void CastRtpStream::GetRawEvents(
    const base::Callback<void(scoped_ptr<base::BinaryValue>)>& callback,
    const std::string& extra_data) {
  VLOG(1) << "CastRtpStream::GetRawEvents = "
          << (IsAudio() ? "audio" : "video");
  cast_session_->GetEventLogsAndReset(IsAudio(), extra_data, callback);
}

void CastRtpStream::GetStats(
    const base::Callback<void(scoped_ptr<base::DictionaryValue>)>& callback) {
  VLOG(1) << "CastRtpStream::GetStats = "
          << (IsAudio() ? "audio" : "video");
  cast_session_->GetStatsAndReset(IsAudio(), callback);
}

bool CastRtpStream::IsAudio() const {
  return track_.source().type() == blink::WebMediaStreamSource::TypeAudio;
}

void CastRtpStream::DidEncounterError(const std::string& message) {
  VLOG(1) << "CastRtpStream::DidEncounterError(" << message << ") = "
          << (IsAudio() ? "audio" : "video");
  // Save the WeakPtr first because the error callback might delete this object.
  base::WeakPtr<CastRtpStream> ptr = weak_factory_.GetWeakPtr();
  error_callback_.Run(message);
  content::RenderThread::Get()->GetMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&CastRtpStream::Stop, ptr));
}
