// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_MANAGER_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_MANAGER_MOCK_H_

#include "base/basictypes.h"
#include "gpu/command_buffer/service/stream_texture_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class StreamTexture;

class MockStreamTextureManager : public StreamTextureManager {
 public:
  MockStreamTextureManager();
  virtual ~MockStreamTextureManager();

  MOCK_METHOD2(CreateStreamTexture, GLuint(uint32 service_id,
                                           uint32 client_id));
  MOCK_METHOD1(DestroyStreamTexture, void(uint32 service_id));
  MOCK_METHOD1(LookupStreamTexture, StreamTexture*(uint32 service_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStreamTextureManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_MANAGER_MOCK_H_
