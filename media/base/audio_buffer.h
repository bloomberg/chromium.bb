// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_BUFFER_H_
#define MEDIA_BASE_AUDIO_BUFFER_H_

#include <vector>

#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "media/base/media_export.h"
#include "media/base/sample_format.h"

namespace media {
class AudioBus;

// An audio buffer that takes a copy of the data passed to it, holds it, and
// copies it into an AudioBus when needed. Also supports an end of stream
// marker.
class MEDIA_EXPORT AudioBuffer
    : public base::RefCountedThreadSafe<AudioBuffer> {
 public:
  // Create an AudioBuffer whose channel data is copied from |data|. For
  // interleaved data, only the first buffer is used. For planar data, the
  // number of buffers must be equal to |channel_count|. |frame_count| is the
  // number of frames in each buffer. |data| must not be null and |frame_count|
  // must be >= 0.
  //
  // TODO(jrummell): Compute duration rather than pass it in.
  static scoped_refptr<AudioBuffer> CopyFrom(SampleFormat sample_format,
                                             int channel_count,
                                             int frame_count,
                                             const uint8* const* data,
                                             const base::TimeDelta timestamp,
                                             const base::TimeDelta duration);

  // Create a AudioBuffer indicating we've reached end of stream.
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<AudioBuffer> CreateEOSBuffer();

  // Copy frames into |dest|. |frames_to_copy| is the number of frames to copy.
  // |source_frame_offset| specified how many frames in the buffer to skip
  // first. |dest_frame_offset| is the frame offset in |dest|. The frames are
  // converted from their source format into planar float32 data (which is all
  // that AudioBus handles).
  void ReadFrames(int frames_to_copy,
                  int source_frame_offset,
                  int dest_frame_offset,
                  AudioBus* dest);

  // Return the number of frames held.
  int frame_count() const { return frame_count_; }

  // Access to constructor parameters.
  base::TimeDelta timestamp() const { return timestamp_; }
  base::TimeDelta duration() const { return duration_; }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const { return data_ == NULL; }

 private:
  friend class base::RefCountedThreadSafe<AudioBuffer>;

  // Allocates aligned contiguous buffer to hold all channel data (1 block for
  // interleaved data, |channel_count| blocks for planar data), copies
  // [data,data+data_size) to the allocated buffer(s). If |data| is null an end
  // of stream buffer is created.
  AudioBuffer(SampleFormat sample_format,
              int channel_count,
              int frame_count,
              const uint8* const* data,
              const base::TimeDelta timestamp,
              const base::TimeDelta duration);

  virtual ~AudioBuffer();

  SampleFormat sample_format_;
  int channel_count_;
  int frame_count_;
  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  // Contiguous block of channel data.
  scoped_ptr_malloc<uint8, base::ScopedPtrAlignedFree> data_;

  // For planar data, points to each channels data.
  std::vector<uint8*> channel_data_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_BUFFER_H_
