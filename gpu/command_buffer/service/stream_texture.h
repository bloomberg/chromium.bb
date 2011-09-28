// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_H_
#pragma once

#include "base/basictypes.h"

namespace gpu {

class StreamTexture {
 public:
  StreamTexture() {
  }

  virtual ~StreamTexture() {
  }

  virtual void Update() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamTexture);
};

} // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_H_
