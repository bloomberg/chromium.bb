// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/webm_muxer.h"

#include <limits>

namespace media {

WebmMuxer::WebmMuxer(const WriteDataCB& write_data_callback)
    : write_data_callback_(write_data_callback),
      position_(0) {
  DCHECK(thread_checker_.CalledOnValidThread());
  segment_.Init(this);
  segment_.set_mode(mkvmuxer::Segment::kLive);
  segment_.OutputCues(false);

  mkvmuxer::SegmentInfo* const info = segment_.GetSegmentInfo();
  info->set_writing_app("Chrome");
  info->set_muxing_app("Chrome");
}

WebmMuxer::~WebmMuxer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  segment_.Finalize();
}

uint64_t WebmMuxer::AddVideoTrack(const gfx::Size& frame_size,
                                  double frame_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const uint64_t track_index =
      segment_.AddVideoTrack(frame_size.width(), frame_size.height(), 0);

  mkvmuxer::VideoTrack* const video_track =
      reinterpret_cast<mkvmuxer::VideoTrack*>(
          segment_.GetTrackByNumber(track_index));
  DCHECK(video_track);
  video_track->set_codec_id(mkvmuxer::Tracks::kVp8CodecId);
  DCHECK_EQ(video_track->crop_right(), 0ull);
  DCHECK_EQ(video_track->crop_left(), 0ull);
  DCHECK_EQ(video_track->crop_top(), 0ull);
  DCHECK_EQ(video_track->crop_bottom(), 0ull);

  video_track->set_frame_rate(frame_rate);
  video_track->set_default_duration(base::Time::kMicrosecondsPerSecond /
                                    frame_rate);
  // Segment's timestamps should be in milliseconds, DCHECK it. See
  // http://www.webmproject.org/docs/container/#muxer-guidelines
  DCHECK_EQ(segment_.GetSegmentInfo()->timecode_scale(), 1000000ull);

  return track_index;
}

void WebmMuxer::OnEncodedVideo(uint64_t track_number,
                               const base::StringPiece& encoded_data,
                               base::TimeDelta timestamp,
                               bool keyframe) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // |track_number|, a caller-side identifier, cannot be zero (!), see
  // http://www.matroska.org/technical/specs/index.html#Tracks
  DCHECK_GT(track_number, 0u);
  DCHECK(segment_.GetTrackByNumber(track_number));

  segment_.AddFrame(reinterpret_cast<const uint8_t*>(encoded_data.data()),
      encoded_data.size(), track_number, timestamp.InMilliseconds(), keyframe);
}

mkvmuxer::int32 WebmMuxer::Write(const void* buf, mkvmuxer::uint32 len) {
  DCHECK(thread_checker_.CalledOnValidThread());
  write_data_callback_.Run(base::StringPiece(reinterpret_cast<const char*>(buf),
                                             len));
  position_ += len;
  return 0;
}

mkvmuxer::int64 WebmMuxer::Position() const {
  return position_.ValueOrDie();
}

mkvmuxer::int32 WebmMuxer::Position(mkvmuxer::int64 position) {
  // The stream is not Seekable() so indicate we cannot set the position.
  return -1;
}

bool WebmMuxer::Seekable() const {
  return false;
}

void WebmMuxer::ElementStartNotify(mkvmuxer::uint64 element_id,
                                   mkvmuxer::int64 position) {
  // This method gets pinged before items are sent to |write_data_callback_|.
  DCHECK_GE(position, position_.ValueOrDefault(0))
      << "Can't go back in a live WebM stream.";
}

}  // namespace media
