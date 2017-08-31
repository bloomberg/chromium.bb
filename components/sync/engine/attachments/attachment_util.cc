// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/attachment_util.h"

#include "base/memory/ref_counted_memory.h"
#include "third_party/crc32c/src/include/crc32c/crc32c.h"

namespace syncer {

uint32_t ComputeCrc32c(const scoped_refptr<base::RefCountedMemory>& data) {
  return crc32c::Crc32c(data->front_as<char>(), data->size());
}

}  // namespace syncer
