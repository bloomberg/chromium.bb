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
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_sender.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"

using media::cast::AudioSenderConfig;
using media::cast::VideoSenderConfig;

namespace {
const char kCodecNameOpus[] = "OPUS";
const char kCodecNameVp8[] = "VP8";

CastRtpPayloadParams DefaultOpusPayload() {
  CastRtpPayloadParams payload;
  payload.payload_type = 111;
  payload.codec_name = kCodecNameOpus;
  payload.clock_rate = 48000;
  payload.channels = 2;
  payload.min_bitrate = payload.max_bitrate =
      media::cast::kDefaultAudioEncoderBitrate;
  return payload;
}

CastRtpPayloadParams DefaultVp8Payload() {
  CastRtpPayloadParams payload;
  payload.payload_type = 100;
  payload.codec_name = kCodecNameVp8;
  payload.clock_rate = 90000;
  payload.width = 1280;
  payload.height = 720;
  payload.min_bitrate = 50 * 1000;
  payload.max_bitrate = 2000 * 1000;
  return payload;
}

CastRtpCaps DefaultAudioCaps() {
  CastRtpCaps caps;
  caps.payloads.push_back(DefaultOpusPayload());
  // TODO(hclam): Fill in |rtcp_features| and |fec_mechanisms|.
  return caps;
}

CastRtpCaps DefaultVideoCaps() {
  CastRtpCaps caps;
  caps.payloads.push_back(DefaultVp8Payload());
  // TODO(hclam): Fill in |rtcp_features| and |fec_mechanisms|.
  return caps;
}

bool ToAudioSenderConfig(const CastRtpParams& params,
                         AudioSenderConfig* config) {
  if (params.payloads.empty())
    return false;
  const CastRtpPayloadParams& payload_params = params.payloads[0];
  config->sender_ssrc = payload_params.ssrc;
  config->use_external_encoder = false;
  config->frequency = payload_params.clock_rate;
  config->channels = payload_params.channels;
  config->bitrate = payload_params.max_bitrate;
  config->codec = media::cast::kPcm16;
  if (payload_params.codec_name == kCodecNameOpus)
    config->codec = media::cast::kOpus;
  else
    return false;
  return true;
}

bool ToVideoSenderConfig(const CastRtpParams& params,
                         VideoSenderConfig* config) {
  if (params.payloads.empty())
    return false;
  const CastRtpPayloadParams& payload_params = params.payloads[0];
  config->sender_ssrc = payload_params.ssrc;
  config->use_external_encoder = false;
  config->width = payload_params.width;
  config->height = payload_params.height;
  config->min_bitrate = config->start_bitrate = payload_params.min_bitrate;
  config->max_bitrate = payload_params.max_bitrate;
  if (payload_params.codec_name == kCodecNameVp8)
    config->codec = media::cast::kVp8;
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
  explicit CastVideoSink(const blink::WebMediaStreamTrack& track)
      : track_(track), sink_added_(false) {
  }

  virtual ~CastVideoSink() {
    if (sink_added_)
      RemoveFromVideoTrack(this, track_);
  }

  // content::MediaStreamVideoSink implementation.
  virtual void OnVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE {
    // TODO(hclam): Pass in the accurate capture time to have good
    // audio/video sync.
    frame_input_->InsertRawVideoFrame(frame,
                                      base::TimeTicks::Now());
  }

  // Attach this sink to MediaStreamTrack. This method call must
  // be made on the render thread. Incoming data can then be
  // passed to media::cast::FrameInput on any thread.
  void AddToTrack(
      const scoped_refptr<media::cast::FrameInput>& frame_input) {
    DCHECK(!sink_added_);
    frame_input_ = frame_input;
    AddToVideoTrack(this, track_);
    sink_added_ = true;
  }

 private:
  blink::WebMediaStreamTrack track_;
  scoped_refptr<media::cast::FrameInput> frame_input_;
  bool sink_added_;

  DISALLOW_COPY_AND_ASSIGN(CastVideoSink);
};

// Receives audio data from a MediaStreamTrack. Data is submitted to
// media::cast::FrameInput.
//
// Threading: Audio frames are received on the real-time audio thread.
class CastAudioSink : public base::SupportsWeakPtr<CastAudioSink>,
                      public content::MediaStreamAudioSink {
 public:
  explicit CastAudioSink(const blink::WebMediaStreamTrack& track)
      : track_(track), sink_added_(false) {
  }

  virtual ~CastAudioSink() {
    if (sink_added_)
      RemoveFromAudioTrack(this, track_);
  }

  // content::MediaStreamAudioSink implementation.
  virtual void OnData(const int16* audio_data,
                      int sample_rate,
                      int number_of_channels,
                      int number_of_frames) OVERRIDE {
    scoped_ptr<media::AudioBus> audio_bus(
        media::AudioBus::Create(number_of_channels,
                                number_of_frames));
    audio_bus->FromInterleaved(audio_data, number_of_frames, 2);

    // TODO(hclam): Pass in the accurate capture time to have good
    // audio/video sync.
    media::AudioBus* audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        base::TimeTicks::Now(),
        base::Bind(&DeleteAudioBus, base::Passed(&audio_bus)));
  }

  virtual void OnSetFormat(
      const media::AudioParameters& params) OVERRIDE{
    NOTIMPLEMENTED();
  }

  // See CastVideoSink for details.
  void AddToTrack(
      const scoped_refptr<media::cast::FrameInput>& frame_input) {
    DCHECK(!sink_added_);
    frame_input_ = frame_input;
    AddToAudioTrack(this, track_);
    sink_added_ = true;
  }

 private:
  blink::WebMediaStreamTrack track_;
  scoped_refptr<media::cast::FrameInput> frame_input_;
  bool sink_added_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioSink);
};

CastCodecSpecificParams::CastCodecSpecificParams() {
}

CastCodecSpecificParams::~CastCodecSpecificParams() {
}

CastRtpPayloadParams::CastRtpPayloadParams()
    : payload_type(0),
      ssrc(0),
      clock_rate(0),
      max_bitrate(0),
      min_bitrate(0),
      channels(0),
      width(0),
      height(0) {
}

CastRtpPayloadParams::~CastRtpPayloadParams() {
}

CastRtpCaps::CastRtpCaps() {
}

CastRtpCaps::~CastRtpCaps() {
}

CastRtpStream::CastRtpStream(
    const blink::WebMediaStreamTrack& track,
    const scoped_refptr<CastSession>& session)
    : track_(track),
      cast_session_(session) {
}

CastRtpStream::~CastRtpStream() {
}

CastRtpCaps CastRtpStream::GetCaps() {
  if (IsAudio())
    return DefaultAudioCaps();
  else
    return DefaultVideoCaps();
}

CastRtpParams CastRtpStream::GetParams() {
  return params_;
}

void CastRtpStream::Start(const CastRtpParams& params) {
  if (IsAudio()) {
    AudioSenderConfig config;
    if (!ToAudioSenderConfig(params, &config)) {
      DVLOG(1) << "Invalid parameters for audio.";
    }
    audio_sink_.reset(new CastAudioSink(track_));
    cast_session_->StartAudio(
        config,
        base::Bind(&CastAudioSink::AddToTrack,
                   audio_sink_->AsWeakPtr()));
  } else {
    VideoSenderConfig config;
    if (!ToVideoSenderConfig(params, &config)) {
      DVLOG(1) << "Invalid parameters for video.";
    }
    video_sink_.reset(new CastVideoSink(track_));
    cast_session_->StartVideo(
        config,
        base::Bind(&CastVideoSink::AddToTrack,
                   video_sink_->AsWeakPtr()));
  }
}

void CastRtpStream::Stop() {
  audio_sink_.reset();
  video_sink_.reset();
}

bool CastRtpStream::IsAudio() const {
  return track_.source().type() == blink::WebMediaStreamSource::TypeAudio;
}
