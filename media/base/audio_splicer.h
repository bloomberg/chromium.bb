// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_SPLICER_H_
#define MEDIA_BASE_AUDIO_SPLICER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

class AudioBuffer;
class AudioBus;
class AudioStreamSanitizer;

// Helper class that handles filling gaps and resolving overlaps.
class MEDIA_EXPORT AudioSplicer {
 public:
  explicit AudioSplicer(int samples_per_second);
  ~AudioSplicer();

  // Resets the splicer state by clearing the output buffers queue and resetting
  // the timestamp helper.
  void Reset();

  // Adds a new buffer full of samples or end of stream buffer to the splicer.
  // Returns true if the buffer was accepted.  False is returned if an error
  // occurred.
  bool AddInput(const scoped_refptr<AudioBuffer>& input);

  // Returns true if the splicer has a buffer to return.
  bool HasNextBuffer() const;

  // Removes the next buffer from the output buffer queue and returns it; this
  // should only be called if HasNextBuffer() returns true.
  scoped_refptr<AudioBuffer> GetNextBuffer();

  // Indicates that overlapping buffers are coming up and should be crossfaded.
  // Once set, all buffers encountered after |splice_timestamp| will be queued
  // internally until at least 5ms of overlapping buffers are received (or end
  // of stream, whichever comes first).
  void SetSpliceTimestamp(base::TimeDelta splice_timestamp);

 private:
  friend class AudioSplicerTest;

  // Extracts frames to be crossfaded from |pre_splice_sanitizer_|.  Transfers
  // all frames before |splice_timestamp_| into |output_sanitizer_| and drops
  // frames outside of the crossfade duration.
  //
  // The size of the returned AudioBus is the crossfade duration in frames.
  // Crossfade duration is calculated based on the number of frames available
  // after |splice_timestamp_| in each sanitizer and capped by
  // |max_crossfade_duration_|.
  //
  // |pre_splice_sanitizer_| will be empty after this operation.
  scoped_ptr<AudioBus> ExtractCrossfadeFromPreSplice();

  // Crossfades |pre_splice_bus->frames()| frames from |post_splice_sanitizer_|
  // with those from |pre_splice_bus|.  Adds the crossfaded buffer to
  // |output_sanitizer_| along with all buffers in |post_splice_sanitizer_|.
  //
  // |post_splice_sanitizer_| will be empty after this operation.
  void CrossfadePostSplice(scoped_ptr<AudioBus> pre_splice_bus);

  const base::TimeDelta max_crossfade_duration_;
  base::TimeDelta splice_timestamp_;

  // The various sanitizers for each stage of the crossfade process.  Buffers in
  // |output_sanitizer_| are immediately available for consumption by external
  // callers.
  //
  // Overlapped buffers go into the |pre_splice_sanitizer_| while overlapping
  // buffers go into the |post_splice_sanitizer_|.  Once enough buffers for
  // crossfading are received the pre and post sanitizers are drained into
  // |output_sanitizer_| by the two ExtractCrossfadeFromXXX methods above.
  //
  // |pre_splice_sanitizer_| is not constructed until the first splice frame is
  // encountered.  At which point it is constructed based on the timestamp state
  // of |output_sanitizer_|.  It is destructed once the splice is finished.
  scoped_ptr<AudioStreamSanitizer> output_sanitizer_;
  scoped_ptr<AudioStreamSanitizer> pre_splice_sanitizer_;
  scoped_ptr<AudioStreamSanitizer> post_splice_sanitizer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioSplicer);
};

}  // namespace media

#endif
