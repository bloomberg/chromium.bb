// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_RENDERER_H_
#define MEDIA_BASE_VIDEO_RENDERER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class DemuxerStream;
class VideoDecoder;

class MEDIA_EXPORT VideoRenderer {
 public:
  // Used to query the current time or duration of the media.
  typedef base::Callback<base::TimeDelta()> TimeDeltaCB;

  VideoRenderer();

  // Stops all operations and fires all pending callbacks.
  virtual ~VideoRenderer();

  // Initializes a VideoRenderer with |stream|, executing |init_cb| upon
  // completion.
  //
  // |statistics_cb| is executed periodically with video rendering stats, such
  // as dropped frames.
  //
  // |buffering_state_cb| is executed when video rendering has either run out of
  // data or has enough data to continue playback.
  //
  // |ended_cb| is executed when video rendering has reached the end of stream.
  //
  // |error_cb| is executed if an error was encountered.
  //
  // |get_time_cb| is used to query the current media playback time.
  virtual void Initialize(DemuxerStream* stream,
                          bool low_delay,
                          const PipelineStatusCB& init_cb,
                          const StatisticsCB& statistics_cb,
                          const BufferingStateCB& buffering_state_cb,
                          const base::Closure& ended_cb,
                          const PipelineStatusCB& error_cb,
                          const TimeDeltaCB& get_time_cb) = 0;

  // Discards any video data and stops reading from |stream|, executing
  // |callback| when completed.
  //
  // Clients should expect |buffering_state_cb| to be called with
  // BUFFERING_HAVE_NOTHING while flushing is in progress.
  virtual void Flush(const base::Closure& callback) = 0;

  // Starts playback at |timestamp| by reading from |stream| and decoding and
  // rendering video.
  //
  // Only valid to call after a successful Initialize() or Flush().
  virtual void StartPlayingFrom(base::TimeDelta timestamp) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoRenderer);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_RENDERER_H_
