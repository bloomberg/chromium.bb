// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_shared_buffer.h"

namespace mojo {
namespace system {

RawSharedBuffer::~RawSharedBuffer() {
}

// static
scoped_ptr<RawSharedBuffer> RawSharedBuffer::Create(size_t num_bytes) {
  if (!num_bytes)
    return scoped_ptr<RawSharedBuffer>();

  scoped_ptr<RawSharedBuffer> rv(new RawSharedBuffer(num_bytes));
  if (!rv->Init())
    return scoped_ptr<RawSharedBuffer>();
  return rv.Pass();
}

scoped_ptr<RawSharedBuffer::Mapping> RawSharedBuffer::Map(size_t offset,
                                                          size_t length) {
  if (offset > num_bytes_ || length == 0)
    return scoped_ptr<Mapping>();

  // Note: This is an overflow-safe check of |offset + length > num_bytes_|
  // (that |num_bytes >= offset| is verified above).
  if (length > num_bytes_ - offset)
    return scoped_ptr<Mapping>();

  return MapImpl(offset, length);
}

RawSharedBuffer::RawSharedBuffer(size_t num_bytes) : num_bytes_(num_bytes) {
}

}  // namespace system
}  // namespace mojo
