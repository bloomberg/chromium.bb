// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../common/command_buffer.h"
#include "../common/logging.h"

namespace gpu {

ReadWriteTokens::ReadWriteTokens()
    : last_token_read(-1), last_token_written(-1) {
}

ReadWriteTokens::ReadWriteTokens(int32 read, int32 written)
    : last_token_read(read), last_token_written(written) {
}

bool ReadWriteTokens::InRange(int32 token) const {
  int32 min = last_token_read;
  int32 max = last_token_written;
  GPU_DCHECK_GE(min, 0);
  GPU_DCHECK_GE(max, 0);
  if (min <= max) {
    // token should be in [min .. max)
    return (token >= min) && (token < max);
  }
  // token should be in [0 .. max) or [min .. wrap token)
  return (token >= min) || (token < max);
}

}  // namespace gpu
