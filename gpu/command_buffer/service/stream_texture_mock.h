// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_MOCK_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/stream_texture.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class MockStreamTexture : public StreamTexture {
 public:
  MockStreamTexture();
  virtual ~MockStreamTexture();

  MOCK_METHOD0(Update, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStreamTexture);
};

} // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_MOCK_H_
