// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_rtp_stream.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/bind_to_current_loop.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_sender.h"
#include "media/cast/transport/cast_transport_config.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"

using media::cast::AudioSenderConfig;
using media::cast::VideoSenderConfig;

namespace {
const char kCodecNameOpus[] = "OPUS";
const char kCodecNameVp8[] = "VP8";

CastRtpPayloadParams DefaultOpusPayload() {
  CastRtpPayloadParams payload;
  payload.ssrc = 1;
  payload.feedback_ssrc = 1;
  payload.payload_type = 127;
  payload.codec_name = kCodecNameOpus;
  payload.clock_rate = 48000;
  payload.channels = 2;
  payload.min_bitrate = payload.max_bitrate =
      media::cast::kDefaultAudioEncoderBitrate;
  return payload;
}

CastRtpPayloadParams DefaultVp8Payload() {
  CastRtpPayloadParams payload;
  payload.ssrc = 11;
  payload.feedback_ssrc = 12;
  payload.payload_type = 96;
  payload.codec_name = kCodecNameVp8;
  payload.clock_rate = 90000;
  payload.width = 1280;
  payload.height = 720;
  payload.min_bitrate = 50 * 1000;
  payload.max_bitrate = 2000 * 1000;
  return payload;
}

std::vector<CastRtpParams> SupportedAudioParams() {
  // TODO(hclam): Fill in more codecs here.
  std::vector<CastRtpParams> supported_params;
  supported_params.push_back(CastRtpParams(DefaultOpusPayload()));
  return supported_params;
}

std::vector<CastRtpParams> SupportedVideoParams() {
  // TODO(hclam): Fill in H264 here.
  std::vector<CastRtpParams> supported_params;
  supported_params.push_back(CastRtpParams(DefaultVp8Payload()));
  return supported_params;
}

bool ToAudioSenderConfig(const CastRtpParams& params,
                         AudioSenderConfig* config) {
  config->sender_ssrc = params.payload.ssrc;
  config->incoming_feedback_ssrc = params.payload.feedback_ssrc;
  config->rtp_config.payload_type = params.payload.payload_type;
  config->use_external_encoder = false;
  config->frequency = params.payload.clock_rate;
  config->channels = params.payload.channels;
  config->bitrate = params.payload.max_bitrate;
  config->codec = media::cast::transport::kPcm16;
  if (params.payload.codec_name == kCodecNameOpus)
    config->codec = media::cast::transport::kOpus;
  else
    return false;
  return true;
}

bool ToVideoSenderConfig(const CastRtpParams& params,
                         VideoSenderConfig* config) {
  config->sender_ssrc = params.payload.ssrc;
  config->incoming_feedback_ssrc = params.payload.feedback_ssrc;
  config->rtp_config.payload_type = params.payload.payload_type;
  config->use_external_encoder = false;
  config->width = params.payload.width;
  config->height = params.payload.height;
  config->min_bitrate = config->start_bitrate = params.payload.min_bitrate;
  config->max_bitrate = params.payload.max_bitrate;
  if (params.payload.codec_name == kCodecNameVp8)
    config->codec = media::cast::transport::kVp8;
  else
    return false;
  return true;
}

void DeleteAudioBus(scoped_ptr<media::AudioBus> audio_bus) {
  // Do nothing as |audio_bus| will be deleted.
}

}  // namespace

// This class receives MediaStreamTrack events and video frames from a
// MediaStreamTrack. Video frames are submitted to media::cast::FrameInput.
//
// Threading: Video frames are received on the render thread.
class CastVideoSink : public base::SupportsWeakPtr<CastVideoSink>,
                      public content::MediaStreamVideoSink {
 public:
  // |track| provides data for this sink.
  // |error_callback| is called if video formats don't match.
  CastVideoSink(const blink::WebMediaStreamTrack& track,
                const CastRtpStream::ErrorCallback& error_callback)
      : track_(track),
        sink_added_(false),
        error_callback_(error_callback),
        render_thread_task_runner_(content::RenderThread::Get()
                                       ->GetMessageLoop()
                                       ->message_loop_proxy()) {}

  virtual ~CastVideoSink() {
    if (sink_added_)
      RemoveFromVideoTrack(this, track_);
  }

  // content::MediaStreamVideoSink implementation.
  virtual void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE {
    DCHECK(render_thread_task_runner_->BelongsToCurrentThread());
    DCHECK(frame_input_);
    // TODO(hclam): Pass in the accurate capture time to have good
    // audio/video sync.
    frame_input_->InsertRawVideoFrame(frame, base::TimeTicks::Now());
  }

  // Attach this sink to MediaStreamTrack. This method call must
  // be made on the render thread. Incoming data can then be
  // passed to media::cast::FrameInput on any thread.
  void AddToTrack(const scoped_refptr<media::cast::FrameInput>& frame_input) {
    DCHECK(render_thread_task_runner_->BelongsToCurrentThread());

    frame_input_ = frame_input;
    if (!sink_added_) {
      AddToVideoTrack(this, track_);
      sink_added_ = true;
    }
  }

 private:
  blink::WebMediaStreamTrack track_;
  scoped_refptr<media::cast::FrameInput> frame_input_;
  bool sink_added_;
  CastRtpStream::ErrorCallback error_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> render_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CastVideoSink);
};

// Receives audio data from a MediaStreamTrack. Data is submitted to
// media::cast::FrameInput.
//
// Threading: Audio frames are received on the real-time audio thread.
class CastAudioSink : public base::SupportsWeakPtr<CastAudioSink>,
                      public content::MediaStreamAudioSink {
 public:
  // |track| provides data for this sink.
  // |error_callback| is called if audio formats don't match.
  CastAudioSink(const blink::WebMediaStreamTrack& track,
                const CastRtpStream::ErrorCallback& error_callback)
      : track_(track),
        sink_added_(false),
        error_callback_(error_callback),
        weak_factory_(this),
        render_thread_task_runner_(content::RenderThread::Get()
                                       ->GetMessageLoop()
                                       ->message_loop_proxy()) {}

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
    scoped_ptr<media::AudioBus> audio_bus(
        media::AudioBus::Create(number_of_channels, number_of_frames));
    audio_bus->FromInterleaved(audio_data, number_of_frames, 2);

    // TODO(hclam): Pass in the accurate capture time to have good
    // audio / video sync.

    // TODO(hclam): We shouldn't hop through the render thread.
    // Bounce the call from the real-time audio thread to the render thread.
    // Needed since frame_input_ can be changed runtime by the render thread.
    media::AudioBus* const audio_bus_ptr = audio_bus.get();
    render_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CastAudioSink::SendAudio,
                   weak_factory_.GetWeakPtr(),
                   audio_bus_ptr,
                   base::TimeTicks::Now(),
                   base::Bind(&DeleteAudioBus, base::Passed(&audio_bus))));
  }

  void SendAudio(const media::AudioBus* audio_bus_ptr,
                 const base::TimeTicks& recorded_time,
                 const base::Closure& done_callback) {
    DCHECK(render_thread_task_runner_->BelongsToCurrentThread());
    DCHECK(frame_input_);
    frame_input_->InsertAudio(audio_bus_ptr, recorded_time, done_callback);
  }

  // Called on real-time audio thread.
  virtual void OnSetFormat(const media::AudioParameters& params) OVERRIDE {
    NOTIMPLEMENTED();
  }

  // See CastVideoSink for details.
  void AddToTrack(const scoped_refptr<media::cast::FrameInput>& frame_input) {
    DCHECK(render_thread_task_runner_->BelongsToCurrentThread());
    frame_input_ = frame_input;
    if (!sink_added_) {
      AddToAudioTrack(this, track_);
      sink_added_ = true;
    }
  }

 private:
  blink::WebMediaStreamTrack track_;
  scoped_refptr<media::cast::FrameInput> frame_input_;
  bool sink_added_;
  CastRtpStream::ErrorCallback error_callback_;
  base::WeakPtrFactory<CastAudioSink> weak_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> render_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioSink);
};

CastRtpParams::CastRtpParams(const CastRtpPayloadParams& payload_params)
    : payload(payload_params) {}

CastCodecSpecificParams::CastCodecSpecificParams() {}

CastCodecSpecificParams::~CastCodecSpecificParams() {}

CastRtpPayloadParams::CastRtpPayloadParams()
    : payload_type(0),
      ssrc(0),
      feedback_ssrc(0),
      clock_rate(0),
      max_bitrate(0),
      min_bitrate(0),
      channels(0),
      width(0),
      height(0) {
}

CastRtpPayloadParams::~CastRtpPayloadParams() {
}

CastRtpParams::CastRtpParams() {
}

CastRtpParams::~CastRtpParams() {
}

CastRtpStream::CastRtpStream(const blink::WebMediaStreamTrack& track,
                             const scoped_refptr<CastSession>& session)
    : track_(track),
      cast_session_(session),
      weak_factory_(this) {}

CastRtpStream::~CastRtpStream() {
}

std::vector<CastRtpParams> CastRtpStream::GetSupportedParams() {
  if (IsAudio())
    return SupportedAudioParams();
  else
    return SupportedVideoParams();
}

CastRtpParams CastRtpStream::GetParams() {
  return params_;
}

void CastRtpStream::Start(const CastRtpParams& params,
                          const base::Closure& start_callback,
                          const base::Closure& stop_callback,
                          const ErrorCallback& error_callback) {
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
                                            weak_factory_.GetWeakPtr()))));
    cast_session_->StartAudio(
        config,
        base::Bind(&CastAudioSink::AddToTrack,
                   audio_sink_->AsWeakPtr()));
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
        base::Bind(&CastVideoSink::AddToTrack,
                   video_sink_->AsWeakPtr()));
    start_callback.Run();
  }
}

void CastRtpStream::Stop() {
  audio_sink_.reset();
  video_sink_.reset();
  stop_callback_.Run();
}

void CastRtpStream::ToggleLogging(bool enable) {
  cast_session_->ToggleLogging(IsAudio(), enable);
}

void CastRtpStream::GetRawEvents(
    const base::Callback<void(scoped_ptr<std::string>)>& callback) {
  cast_session_->GetEventLogsAndReset(IsAudio(), callback);
}

void CastRtpStream::GetStats(
    const base::Callback<void(scoped_ptr<base::DictionaryValue>)>& callback) {
  cast_session_->GetStatsAndReset(IsAudio(), callback);
}

bool CastRtpStream::IsAudio() const {
  return track_.source().type() == blink::WebMediaStreamSource::TypeAudio;
}

void CastRtpStream::DidEncounterError(const std::string& message) {
  error_callback_.Run(message);
  Stop();
}
