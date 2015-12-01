// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_recorder_handler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/audio_track_recorder.h"
#include "content/renderer/media/video_track_recorder.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/bind_to_current_loop.h"
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
    const blink::WebString& mimeType) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Ensure we can support each passed MIME type.
  const std::string input = mimeType.utf8();  // Must outlive tokenizer!
  base::StringTokenizer tokenizer(input, ",");
  while (tokenizer.GetNext()) {
    // Strip whitespace.
    const std::string token(base::CollapseWhitespaceASCII(
        tokenizer.token(), true /* trim sequences with line breaks*/));
    if (!token.empty() &&
        !base::EqualsCaseInsensitiveASCII(token, "video/vp8") &&
        !base::EqualsCaseInsensitiveASCII(token, "video/vp9") &&
        !base::EqualsCaseInsensitiveASCII(token, "audio/opus") &&
        !base::EqualsCaseInsensitiveASCII(token, "video/webm") &&
        !base::EqualsCaseInsensitiveASCII(token, "audio/webm")) {
      return false;
    }
  }

  return true;
}

bool MediaRecorderHandler::initialize(
    blink::WebMediaRecorderHandlerClient* client,
    const blink::WebMediaStream& media_stream,
    const blink::WebString& mimeType) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Save histogram data so we can see how much MediaStream Recorder is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(WEBKIT_MEDIA_STREAM_RECORDER);

  if (!canSupportMimeType(mimeType)) {
    DLOG(ERROR) << "Can't support type " << mimeType.utf8();
    return false;
  }
  use_vp9_ = mimeType.utf8().compare("video/vp9") == 0;
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

  timeslice_ = TimeDelta::FromMilliseconds(timeslice);
  slice_origin_timestamp_ = TimeTicks::Now();

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks, audio_tracks;
  media_stream_.videoTracks(video_tracks);
  media_stream_.audioTracks(audio_tracks);

#if defined(MEDIA_DISABLE_LIBWEBM)
  LOG(WARNING) << "No muxer available";
  return false;
#endif

  DCHECK(!webm_muxer_);

  if (video_tracks.isEmpty() && audio_tracks.isEmpty()) {
    LOG(WARNING) << __FUNCTION__ << ": no media tracks.";
    return false;
  }

  webm_muxer_.reset(new media::WebmMuxer(
      use_vp9_ ? media::kCodecVP9 : media::kCodecVP8,
      video_tracks.size() > 0, audio_tracks.size() > 0,
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

  if (!audio_tracks.isEmpty()) {
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
  DCHECK(recording_);

  recording_ = false;
  timeslice_ = TimeDelta::FromMilliseconds(0);
  video_recorders_.clear();
  audio_recorders_.clear();
#if !defined(MEDIA_DISABLE_LIBWEBM)
  webm_muxer_.reset();
#endif
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
#if !defined(MEDIA_DISABLE_LIBWEBM)
  if (!webm_muxer_)
    return;
  webm_muxer_->OnEncodedVideo(video_frame, encoded_data.Pass(), timestamp,
                              is_key_frame);
#endif
}

void MediaRecorderHandler::OnEncodedAudio(const media::AudioParameters& params,
                                          scoped_ptr<std::string> encoded_data,
                                          base::TimeTicks timestamp) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (webm_muxer_)
    webm_muxer_->OnEncodedAudio(params, encoded_data.Pass(), timestamp);
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
