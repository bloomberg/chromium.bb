// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ENCODED_BITSTREAM_BUFFER_H_
#define MEDIA_BASE_ENCODED_BITSTREAM_BUFFER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/video/video_encode_types.h"

namespace media {

// General encoded video bitstream buffer.
class MEDIA_EXPORT EncodedBitstreamBuffer :
    public base::RefCountedThreadSafe<EncodedBitstreamBuffer> {
 public:
  EncodedBitstreamBuffer(int buffer_id,
                         uint8* buffer,
                         uint32 size,
                         base::SharedMemoryHandle handle,
                         const media::BufferEncodingMetadata& metadata,
                         const base::Closure& destroy_cb);
  // Accessors for properties.
  int buffer_id() const;
  const uint8* buffer() const;
  uint32 size() const;
  base::SharedMemoryHandle shared_memory_handle() const;
  const media::BufferEncodingMetadata& metadata() const;

 protected:
  // Destructor that deallocates the buffers.
  virtual ~EncodedBitstreamBuffer();
  friend class base::RefCountedThreadSafe<EncodedBitstreamBuffer>;

 private:
  int buffer_id_;
  uint8* buffer_;
  uint32 size_;
  const base::SharedMemoryHandle shared_memory_handle_;
  media::BufferEncodingMetadata metadata_;
  const base::Closure destroy_cb_;

  DISALLOW_COPY_AND_ASSIGN(EncodedBitstreamBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_ENCODED_BITSTREAM_BUFFER_H_
