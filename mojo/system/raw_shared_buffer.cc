// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_shared_buffer.h"

#include "base/logging.h"

namespace mojo {
namespace system {

// static
RawSharedBuffer* RawSharedBuffer::Create(size_t num_bytes) {
  DCHECK_GT(num_bytes, 0u);

  RawSharedBuffer* rv = new RawSharedBuffer(num_bytes);
  // No need to take the lock since we haven't given the object to anyone yet.
  if (!rv->InitNoLock())
    return NULL;

  return rv;
}

scoped_ptr<RawSharedBuffer::Mapping> RawSharedBuffer::Map(size_t offset,
                                                          size_t length) {
  if (!IsValidMap(offset, length))
    return scoped_ptr<Mapping>();

  return MapNoCheck(offset, length);
  base::AutoLock locker(lock_);
  return MapImplNoLock(offset, length);
}

bool RawSharedBuffer::IsValidMap(size_t offset, size_t length) {
  if (offset > num_bytes_ || length == 0)
    return false;

  // Note: This is an overflow-safe check of |offset + length > num_bytes_|
  // (that |num_bytes >= offset| is verified above).
  if (length > num_bytes_ - offset)
    return false;

  return true;
}

scoped_ptr<RawSharedBuffer::Mapping> RawSharedBuffer::MapNoCheck(
    size_t offset,
    size_t length) {
  DCHECK(IsValidMap(offset, length));

  base::AutoLock locker(lock_);
  return MapImplNoLock(offset, length);
}

RawSharedBuffer::RawSharedBuffer(size_t num_bytes) : num_bytes_(num_bytes) {
}

RawSharedBuffer::~RawSharedBuffer() {
}

}  // namespace system
}  // namespace mojo
