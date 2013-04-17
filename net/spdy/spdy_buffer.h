// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_BUFFER_H_
#define NET_SPDY_SPDY_BUFFER_H_

#include <cstddef>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

class IOBuffer;
class SpdyFrame;

// SpdyBuffer is a class to hold data read from or to be written to a
// SPDY connection. It is similar to a DrainableIOBuffer but is not
// ref-counted and will include a way to get notified when Consume()
// is called.
//
// NOTE(akalin): This explicitly does not inherit from IOBuffer to
// avoid the needless ref-counting and to avoid working around the
// fact that IOBuffer member functions are not virtual.
class NET_EXPORT_PRIVATE SpdyBuffer {
 public:
  // Construct with the data in the given frame. Assumes that data is
  // owned by |frame| or outlives it.
  explicit SpdyBuffer(scoped_ptr<SpdyFrame> frame);

  // Construct with a copy of the given raw data. |data| must be
  // non-NULL and |size| must be non-zero.
  SpdyBuffer(const char* data, size_t size);

  ~SpdyBuffer();

  // Returns the remaining (unconsumed) data.
  const char* GetRemainingData() const;

  // Returns the number of remaining (unconsumed) bytes.
  size_t GetRemainingSize() const;

  // Consume the given number of bytes, which must be positive but not
  // greater than GetRemainingSize().
  //
  // TODO(akalin): Add a way to get notified when Consume() is called.
  void Consume(size_t consume_size);

  // Returns an IOBuffer pointing to the data starting at
  // GetRemainingData(). Use with care; the returned IOBuffer must not
  // be used past the lifetime of this object, and it is not updated
  // when Consume() is called.
  IOBuffer* GetIOBufferForRemainingData();

 private:
  const scoped_ptr<SpdyFrame> frame_;
  size_t offset_;

  DISALLOW_COPY_AND_ASSIGN(SpdyBuffer);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_BUFFER_H_
