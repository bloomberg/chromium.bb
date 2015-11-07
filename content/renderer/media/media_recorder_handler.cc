// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_recorder_handler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "content/renderer/media/video_track_recorder.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
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
      weak_factory_(this) {
  DVLOG(3) << __FUNCTION__;
}

MediaRecorderHandler::~MediaRecorderHandler() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Send a |last_in_slice| to our |client_|.
  if (client_)
    client_->writeData(nullptr, 0u, true);
}

bool MediaRecorderHandler::canSupportMimeType(
    const blink::WebString& mimeType) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // TODO(mcasas): So far only empty or "video/vp{8,9}" are supported.
  return mimeType.isEmpty() || mimeType.utf8().compare("video/vp8") == 0 ||
         mimeType.utf8().compare("video/vp9") == 0;
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
  DCHECK(!webm_muxer_);
  DCHECK(timeslice_.is_zero());

  timeslice_ = TimeDelta::FromMilliseconds(timeslice);
  slice_origin_timestamp_ = TimeTicks::Now();

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  media_stream_.videoTracks(video_tracks);

  webm_muxer_.reset(new media::WebmMuxer(
      use_vp9_ ? media::kCodecVP9 : media::kCodecVP8, video_tracks.size() > 0,
      false /* no audio for now - http://crbug.com/528519 */,
      base::Bind(&MediaRecorderHandler::WriteData,
                 weak_factory_.GetWeakPtr())));

  if (video_tracks.isEmpty()) {
    // TODO(mcasas): Add audio_tracks and update the code in this function
    // correspondingly, see http://crbug.com/528519. As of now, only video
    // tracks are supported.
    LOG(WARNING) << "Recording no video tracks is not implemented";
    return false;
  }
  // TODO(mcasas): The muxer API supports only one video track. Extend it to
  // several video tracks, see http://crbug.com/528523.
  LOG_IF(WARNING, video_tracks.size() > 1u) << "Recording multiple video"
      << " tracks is not implemented.  Only recording first video track.";
  const blink::WebMediaStreamTrack& video_track = video_tracks[0];
  if (video_track.isNull())
    return false;

  const VideoTrackRecorder::OnEncodedVideoCB on_encoded_video_cb =
      media::BindToCurrentLoop(base::Bind(&MediaRecorderHandler::OnEncodedVideo,
                                          weak_factory_.GetWeakPtr()));

  video_recorders_.push_back(new VideoTrackRecorder(use_vp9_,
                                                    video_track,
                                                    on_encoded_video_cb));

  recording_ = true;
  return true;
}

void MediaRecorderHandler::stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(recording_);

  recording_ = false;
  timeslice_ = TimeDelta::FromMilliseconds(0);
  video_recorders_.clear();
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
  webm_muxer_->OnEncodedVideo(video_frame, encoded_data.Pass(), timestamp,
                              is_key_frame);
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

}  // namespace content
