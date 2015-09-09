// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_recorder_handler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "content/renderer/media/video_track_recorder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/webm_muxer.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

MediaRecorderHandler::MediaRecorderHandler()
    : recording_(false), client_(nullptr), weak_factory_(this) {
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
  // TODO(mcasas): For the time being only "video/vp8" is supported.
  return mimeType.utf8().compare("video/vp8") == 0;
}

bool MediaRecorderHandler::initialize(
    blink::WebMediaRecorderHandlerClient* client,
    const blink::WebMediaStream& media_stream,
    const blink::WebString& mimeType) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (!canSupportMimeType(mimeType)) {
    DLOG(ERROR) << "Can't support type " << mimeType.utf8();
    return false;
  }
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

  webm_muxer_.reset(new media::WebmMuxer(media::BindToCurrentLoop(base::Bind(
      &MediaRecorderHandler::WriteData, weak_factory_.GetWeakPtr()))));
  DCHECK(webm_muxer_);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  media_stream_.videoTracks(video_tracks);

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
      base::Bind(&media::WebmMuxer::OnEncodedVideo,
                 base::Unretained(webm_muxer_.get()));

  video_recorders_.push_back(new VideoTrackRecorder(video_track,
                                                    on_encoded_video_cb));

  recording_ = true;
  return true;
}

void MediaRecorderHandler::stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(recording_);

  recording_ = false;
  video_recorders_.clear();
  webm_muxer_.reset(NULL);
}

void MediaRecorderHandler::pause() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(recording_);
  recording_ = false;
  NOTIMPLEMENTED();
}

void MediaRecorderHandler::resume() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!recording_);
  recording_ = true;
  NOTIMPLEMENTED();
}

void MediaRecorderHandler::WriteData(const base::StringPiece& data) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  client_->writeData(data.data(), data.size(), false  /* lastInSlice */);
}

void MediaRecorderHandler::OnVideoFrameForTesting(
    const scoped_refptr<media::VideoFrame>& frame,
    const base::TimeTicks& timestamp) {
  for (auto* recorder : video_recorders_)
    recorder->OnVideoFrameForTesting(frame, timestamp);
}

}  // namespace content
