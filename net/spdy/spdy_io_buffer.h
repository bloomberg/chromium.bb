// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_IO_BUFFER_H_
#define NET_SPDY_SPDY_IO_BUFFER_H_

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"

namespace net {

class SpdyStream;

// A class for managing SPDY write buffers.  These write buffers need
// to track the SpdyStream which they are associated with so that the
// session can activate the stream lazily and also notify the stream
// on completion of the write.
class NET_EXPORT_PRIVATE SpdyIOBuffer {
 public:
  SpdyIOBuffer();

  // Constructor
  // |buffer| is the actual data buffer.
  // |size| is the size of the data buffer.
  // |stream| is a pointer to the stream which is managing this buffer
  // (can be NULL if the write is for the session itself).
  SpdyIOBuffer(IOBuffer* buffer, int size, SpdyStream* stream);

  // Declare this instead of using the default so that we avoid needing to
  // include spdy_stream.h.
  SpdyIOBuffer(const SpdyIOBuffer& rhs);

  ~SpdyIOBuffer();

  // Declare this instead of using the default so that we avoid needing to
  // include spdy_stream.h.
  SpdyIOBuffer& operator=(const SpdyIOBuffer& rhs);

  void Swap(SpdyIOBuffer* other);

  void Release();

  // Accessors.
  DrainableIOBuffer* buffer() const { return buffer_; }
  const scoped_refptr<SpdyStream>& stream() const { return stream_; }

 private:
  scoped_refptr<DrainableIOBuffer> buffer_;
  scoped_refptr<SpdyStream> stream_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_IO_BUFFER_H_
