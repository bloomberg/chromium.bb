// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PTS_STREAM_H_
#define MEDIA_BASE_PTS_STREAM_H_

// Under some conditions the decoded frames can get invalid or wrong timestamps:
//    - compressed frames are often in decode timestamp (dts) order, which
//      may not always be in presentation timestamp (pts) order;
//    - decoder may report invalid timestamps for the decoded frames;
//    - parser may report invalid timestamps for the compressed frames.
//
// To ensure that the decoded frames are displayed in the proper order, the
// PtsStream class assembles the time information from different sources and
// combines it into the "best guess" timestamp and duration for the current
// frame. Data inside the decoded frame (if provided) is trusted the most,
// followed by data from the packet stream. Estimation based on the last known
// PTS and frame rate is reserved as a last-ditch effort.

#include "base/time.h"
#include "media/base/pts_heap.h"

namespace media {

class StreamSample;

class MEDIA_EXPORT PtsStream {
 public:
  PtsStream();
  ~PtsStream();

  // Initializes an instance using |frame_duration| as default. In absence of
  // other PTS information PtsStream will produce timestamps separated in time
  // by this duration.
  void Initialize(const base::TimeDelta& frame_duration);

  // Sets the |current_pts_| to specified |timestamp| and flushes all enqueued
  // timestamps.
  void Seek(const base::TimeDelta& timestamp);

  // Clears the PTS queue.
  void Flush();

  // Puts timestamp from the stream packet |sample| into a queue, which is used
  // as PTS source if decoded frames don't have a valid timestamp. Only valid
  // timestamps are enqueued.
  void EnqueuePts(StreamSample* sample);

  // Combines data from the decoded |sample|, PTS queue, and PTS estimator
  // into the final PTS and duration.
  void UpdatePtsAndDuration(StreamSample* sample);

  base::TimeDelta current_pts() const { return current_pts_; }
  base::TimeDelta current_duration() const { return current_duration_; }

 private:
  base::TimeDelta default_duration_;  // Frame duration based on the frame rate.

  PtsHeap pts_heap_;  // Heap of presentation timestamps.

  base::TimeDelta current_pts_;
  base::TimeDelta current_duration_;

  DISALLOW_COPY_AND_ASSIGN(PtsStream);
};

}  // namespace media

#endif  // MEDIA_BASE_PTS_STREAM_H_
