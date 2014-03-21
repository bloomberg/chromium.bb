// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_shared_buffer.h"

#include "base/logging.h"

namespace mojo {
namespace system {

// RawSharedBuffer::Mapping ----------------------------------------------------

void RawSharedBuffer::Mapping::Unmap() {
  // TODO(vtl)
  NOTIMPLEMENTED();
}

// RawSharedBuffer -------------------------------------------------------------

bool RawSharedBuffer::Init() {
  // TODO(vtl)
  NOTIMPLEMENTED();
  return false;
}

scoped_ptr<RawSharedBuffer::Mapping> RawSharedBuffer::MapImpl(size_t offset,
                                                              size_t length) {
  // TODO(vtl)
  NOTIMPLEMENTED();
  return scoped_ptr<Mapping>();
}

}  // namespace system
}  // namespace mojo
