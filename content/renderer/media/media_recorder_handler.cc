// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_recorder_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/audio_track_recorder.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/video_track_recorder.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/mime_util.h"
#include "media/base/video_frame.h"
#include "media/capture/webm_muxer.h"
#include "third_party/WebKit/public/platform/WebMediaRecorderHandlerClient.h"
#include "third_party/WebKit/public/platform/WebString.h"

using base::TimeDelta;
using base::TimeTicks;

namespace content {

MediaRecorderHandler::MediaRecorderHandler()
    : use_vp9_(false),
      recording_(false),
      client_(nullptr),
      weak_factory_(this) {}

MediaRecorderHandler::~MediaRecorderHandler() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Send a |last_in_slice| to our |client_|.
  if (client_)
    client_->writeData(nullptr, 0u, true);
}

bool MediaRecorderHandler::canSupportMimeType(
    const blink::WebString& web_type, const blink::WebString& web_codecs) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // An empty |web_type| means MediaRecorderHandler can choose its preferred
  // codecs.
  if (web_type.isEmpty())
    return true;

  const std::string type(web_type.utf8());
  const bool video = base::EqualsCaseInsensitiveASCII(type, "video/webm");
  const bool audio =
      video ? false : base::EqualsCaseInsensitiveASCII(type, "audio/webm");
  if (!video && !audio)
    return false;

  // Both |video| and |audio| support empty |codecs|; |type| == "video" supports
  // vp8, vp9 or opus; |type| = "audio", supports only opus.
  // http://www.webmproject.org/docs/container Sec:"HTML5 Video Type Parameters"
  static const char* kVideoCodecs[] = { "vp8", "vp9", "opus" };
  static const char* kAudioCodecs[] = { "opus" };
  const char** codecs = video ? &kVideoCodecs[0] : &kAudioCodecs[0];
  int codecs_count =  video ? arraysize(kVideoCodecs) : arraysize(kAudioCodecs);

  std::vector<std::string> codecs_list;
  media::ParseCodecString(web_codecs.utf8(), &codecs_list, true /* strip */);
  for (const auto& codec : codecs_list) {
    const auto found = std::find_if(
        &codecs[0], &codecs[codecs_count], [&codec](const char* name) {
          return base::EqualsCaseInsensitiveASCII(codec, name);
        });
    if (found == &codecs[codecs_count])
      return false;
  }
  return true;
}

bool MediaRecorderHandler::initialize(
    blink::WebMediaRecorderHandlerClient* client,
    const blink::WebMediaStream& media_stream,
    const blink::WebString& type,
    const blink::WebString& codecs) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Save histogram data so we can see how much MediaStream Recorder is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(WEBKIT_MEDIA_STREAM_RECORDER);

  if (!canSupportMimeType(type, codecs)) {
    DLOG(ERROR) << "Can't support " << type.utf8()
                << ";codecs=" << codecs.utf8();
    return false;
  }
  use_vp9_ = base::ToLowerASCII(codecs.utf8()).find("vp9") != std::string::npos;
  media_stream_ = media_stream;
  DCHECK(client);
  client_ = client;

  return true;
}

bool MediaRecorderHandler::start() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!recording_);
  return start(0);
}

bool MediaRecorderHandler::start(int timeslice) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!recording_);
  DCHECK(!media_stream_.isNull());
  DCHECK(timeslice_.is_zero());
  DCHECK(!webm_muxer_);

  timeslice_ = TimeDelta::FromMilliseconds(timeslice);
  slice_origin_timestamp_ = TimeTicks::Now();

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks, audio_tracks;
  media_stream_.videoTracks(video_tracks);
  media_stream_.audioTracks(audio_tracks);

  if (video_tracks.isEmpty() && audio_tracks.isEmpty()) {
    LOG(WARNING) << __FUNCTION__ << ": no media tracks.";
    return false;
  }

  const bool use_audio_tracks =
      !audio_tracks.isEmpty() &&
      MediaStreamAudioTrack::GetTrack(audio_tracks[0]);

  webm_muxer_.reset(new media::WebmMuxer(
      use_vp9_ ? media::kCodecVP9 : media::kCodecVP8,
      video_tracks.size() > 0, use_audio_tracks,
      base::Bind(&MediaRecorderHandler::WriteData,
                 weak_factory_.GetWeakPtr())));

  if (!video_tracks.isEmpty()) {
    // TODO(mcasas): The muxer API supports only one video track. Extend it to
    // several video tracks, see http://crbug.com/528523.
    LOG_IF(WARNING, video_tracks.size() > 1u)
        << "Recording multiple video tracks is not implemented. "
        << "Only recording first video track.";
    const blink::WebMediaStreamTrack& video_track = video_tracks[0];
    if (video_track.isNull())
      return false;

    const VideoTrackRecorder::OnEncodedVideoCB on_encoded_video_cb =
        media::BindToCurrentLoop(base::Bind(
            &MediaRecorderHandler::OnEncodedVideo, weak_factory_.GetWeakPtr()));

    video_recorders_.push_back(
        new VideoTrackRecorder(use_vp9_, video_track, on_encoded_video_cb));
  }

  if (use_audio_tracks) {
    // TODO(ajose): The muxer API supports only one audio track. Extend it to
    // several tracks.
    LOG_IF(WARNING, audio_tracks.size() > 1u)
        << "Recording multiple audio"
        << " tracks is not implemented.  Only recording first audio track.";
    const blink::WebMediaStreamTrack& audio_track = audio_tracks[0];
    if (audio_track.isNull())
      return false;

    const AudioTrackRecorder::OnEncodedAudioCB on_encoded_audio_cb =
        media::BindToCurrentLoop(base::Bind(
            &MediaRecorderHandler::OnEncodedAudio, weak_factory_.GetWeakPtr()));

    audio_recorders_.push_back(
        new AudioTrackRecorder(audio_track, on_encoded_audio_cb));
  }

  recording_ = true;
  return true;
}

void MediaRecorderHandler::stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Don't check |recording_| since we can go directly from pause() to stop().

  recording_ = false;
  timeslice_ = TimeDelta::FromMilliseconds(0);
  video_recorders_.clear();
  audio_recorders_.clear();
  webm_muxer_.reset();
}

void MediaRecorderHandler::pause() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(recording_);
  recording_ = false;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Pause();
}

void MediaRecorderHandler::resume() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!recording_);
  recording_ = true;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Resume();
}

void MediaRecorderHandler::OnEncodedVideo(
    const scoped_refptr<media::VideoFrame>& video_frame,
    scoped_ptr<std::string> encoded_data,
    TimeTicks timestamp,
    bool is_key_frame) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (!webm_muxer_)
    return;
  webm_muxer_->OnEncodedVideo(video_frame, std::move(encoded_data), timestamp,
                              is_key_frame);
}

void MediaRecorderHandler::OnEncodedAudio(const media::AudioParameters& params,
                                          scoped_ptr<std::string> encoded_data,
                                          base::TimeTicks timestamp) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (webm_muxer_)
    webm_muxer_->OnEncodedAudio(params, std::move(encoded_data), timestamp);
}

void MediaRecorderHandler::WriteData(base::StringPiece data) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  // Non-buffered mode does not need to check timestamps.
  if (timeslice_.is_zero()) {
    client_->writeData(data.data(), data.length(), true  /* lastInSlice */);
    return;
  }

  const TimeTicks now = TimeTicks::Now();
  const bool last_in_slice = now > slice_origin_timestamp_ + timeslice_;
  DVLOG_IF(1, last_in_slice) << "Slice finished @ " << now;
  if (last_in_slice)
    slice_origin_timestamp_ = now;
  client_->writeData(data.data(), data.length(), last_in_slice);
}

void MediaRecorderHandler::OnVideoFrameForTesting(
    const scoped_refptr<media::VideoFrame>& frame,
    const TimeTicks& timestamp) {
  for (auto* recorder : video_recorders_)
    recorder->OnVideoFrameForTesting(frame, timestamp);
}

void MediaRecorderHandler::OnAudioBusForTesting(
    const media::AudioBus& audio_bus,
    const base::TimeTicks& timestamp) {
  for (auto* recorder : audio_recorders_)
    recorder->OnData(audio_bus, timestamp);
}

void MediaRecorderHandler::SetAudioFormatForTesting(
    const media::AudioParameters& params) {
  for (auto* recorder : audio_recorders_)
    recorder->OnSetFormat(params);
}

}  // namespace content
