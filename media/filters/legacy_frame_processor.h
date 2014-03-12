// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_LEGACY_FRAME_PROCESSOR_H_
#define MEDIA_FILTERS_LEGACY_FRAME_PROCESSOR_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/filters/frame_processor_base.h"

namespace media {

// Helper class that implements Media Source Extension's coded frame processing
// algorithm.
class MEDIA_EXPORT LegacyFrameProcessor : public FrameProcessorBase {
 public:
  explicit LegacyFrameProcessor(const IncreaseDurationCB& increase_duration_cb);
  virtual ~LegacyFrameProcessor();

  // FrameProcessorBase implementation
  virtual void SetSequenceMode(bool sequence_mode) OVERRIDE;
  virtual bool ProcessFrames(const StreamParser::BufferQueue& audio_buffers,
                             const StreamParser::BufferQueue& video_buffers,
                             const StreamParser::TextBufferQueueMap& text_map,
                             base::TimeDelta append_window_start,
                             base::TimeDelta append_window_end,
                             bool* new_media_segment,
                             base::TimeDelta* timestamp_offset) OVERRIDE;

 private:
  // Helper function that adds |timestamp_offset| to each buffer in |buffers|.
  void AdjustBufferTimestamps(const StreamParser::BufferQueue& buffers,
                              base::TimeDelta timestamp_offset);

  // Filters out buffers that are outside of the append window
  // [|append_window_start|, |append_window_end|). |track|'s
  // "needs random access point" is read and updated as this method filters
  // |buffers|. Buffers that are inside the append window are appended to the
  // end of |filtered_buffers|. |track| must be the track associated with all
  // items in |buffers|. |*new_media_segment| is set true if any of |buffers|
  // are filtered out.
  void FilterWithAppendWindow(base::TimeDelta append_window_start,
                              base::TimeDelta append_window_end,
                              const StreamParser::BufferQueue& buffers,
                              MseTrackBuffer* track,
                              bool* new_media_segment,
                              StreamParser::BufferQueue* filtered_buffers);

  // Helper function that appends |buffers| to |stream| and calls
  // |increase_duration_cb_| to potentially update the duration.
  // Returns true if the append was successful. Returns false if
  // |stream| is NULL or something in |buffers| caused the append to fail.
  bool AppendAndUpdateDuration(ChunkDemuxerStream* stream,
                               const StreamParser::BufferQueue& buffers);

  // Helper function for Legacy ProcessFrames() when new text buffers have been
  // parsed.
  // Applies |timestamp_offset| to all buffers in |buffers|, filters |buffers|
  // with append window, and then appends the modified and filtered buffers to
  // the stream associated with the track having |text_track_id|. If any of
  // |buffers| are filtered out by append window, then |*new_media_segment| is
  // set true.
  // Returns true on a successful call. Returns false if an error occurred while
  // processing the buffers.
  bool OnTextBuffers(StreamParser::TrackId text_track_id,
                     base::TimeDelta append_window_start,
                     base::TimeDelta append_window_end,
                     base::TimeDelta timestamp_offset,
                     const StreamParser::BufferQueue& buffers,
                     bool* new_media_segment);

  DISALLOW_COPY_AND_ASSIGN(LegacyFrameProcessor);
};

}  // namespace media

#endif  // MEDIA_FILTERS_LEGACY_FRAME_PROCESSOR_H_
