// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_BUFFER_H_
#define MEDIA_BASE_AUDIO_BUFFER_H_

#include <vector>

#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
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
  // Alignment of each channel's data; this must match what ffmpeg expects
  // (which may be 0, 16, or 32, depending on the processor). Selecting 32 in
  // order to work on all processors.
  enum { kChannelAlignment = 32 };

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

  // Create an AudioBuffer with |frame_count| frames. Buffer is allocated, but
  // not initialized. Timestamp and duration are set to kNoTimestamp().
  static scoped_refptr<AudioBuffer> CreateBuffer(SampleFormat sample_format,
                                                 int channel_count,
                                                 int frame_count);

  // Create an empty AudioBuffer with |frame_count| frames.
  static scoped_refptr<AudioBuffer> CreateEmptyBuffer(
      int channel_count,
      int frame_count,
      const base::TimeDelta timestamp,
      const base::TimeDelta duration);

  // Create a AudioBuffer indicating we've reached end of stream.
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<AudioBuffer> CreateEOSBuffer();

  // Copy frames into |dest|. |frames_to_copy| is the number of frames to copy.
  // |source_frame_offset| specifies how many frames in the buffer to skip
  // first. |dest_frame_offset| is the frame offset in |dest|. The frames are
  // converted from their source format into planar float32 data (which is all
  // that AudioBus handles).
  void ReadFrames(int frames_to_copy,
                  int source_frame_offset,
                  int dest_frame_offset,
                  AudioBus* dest);

  // Trim an AudioBuffer by removing |frames_to_trim| frames from the start.
  // Timestamp and duration are adjusted to reflect the fewer frames.
  // Note that repeated calls to TrimStart() may result in timestamp() and
  // duration() being off by a few microseconds due to rounding issues.
  void TrimStart(int frames_to_trim);

  // Trim an AudioBuffer by removing |frames_to_trim| frames from the end.
  // Duration is adjusted to reflect the fewer frames.
  void TrimEnd(int frames_to_trim);

  // Return the number of channels.
  int channel_count() const { return channel_count_; }

  // Return the number of frames held.
  int frame_count() const { return adjusted_frame_count_; }

  // Access to constructor parameters.
  base::TimeDelta timestamp() const { return timestamp_; }
  base::TimeDelta duration() const { return duration_; }

  // TODO(jrummell): Remove set_timestamp() and set_duration() once
  // DecryptingAudioDecoder::EnqueueFrames() is changed to set them when
  // creating the buffer. See http://crbug.com/255261.
  void set_timestamp(base::TimeDelta timestamp) { timestamp_ = timestamp; }
  void set_duration(base::TimeDelta duration) { duration_ = duration; }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const { return end_of_stream_; }

  // Access to the raw buffer for ffmpeg to write directly to. Data for planar
  // data is grouped by channel. There is only 1 entry for interleaved formats.
  const std::vector<uint8*>& channel_data() const { return channel_data_; }

 private:
  friend class base::RefCountedThreadSafe<AudioBuffer>;

  // Allocates aligned contiguous buffer to hold all channel data (1 block for
  // interleaved data, |channel_count| blocks for planar data), copies
  // [data,data+data_size) to the allocated buffer(s). If |data| is null, no
  // data is copied. If |create_buffer| is false, no data buffer is created (or
  // copied to).
  AudioBuffer(SampleFormat sample_format,
              int channel_count,
              int frame_count,
              bool create_buffer,
              const uint8* const* data,
              const base::TimeDelta timestamp,
              const base::TimeDelta duration);

  virtual ~AudioBuffer();

  const SampleFormat sample_format_;
  const int channel_count_;
  int adjusted_frame_count_;
  int trim_start_;
  const bool end_of_stream_;
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
