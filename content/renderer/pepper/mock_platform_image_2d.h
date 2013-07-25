// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_MOCK_PLATFORM_IMAGE_2D_H_
#define CONTENT_RENDERER_PEPPER_MOCK_PLATFORM_IMAGE_2D_H_

#include "content/renderer/pepper/plugin_delegate.h"
#include "skia/ext/platform_canvas.h"

namespace content {

class MockPlatformImage2D : public PluginDelegate::PlatformImage2D {
 public:
  MockPlatformImage2D(int width, int height);
  virtual ~MockPlatformImage2D();

  // PlatformImage2D implementation.
  virtual skia::PlatformCanvas* Map() OVERRIDE;
  virtual intptr_t GetSharedMemoryHandle(uint32* byte_count) const OVERRIDE;
  virtual TransportDIB* GetTransportDIB() const OVERRIDE;

 private:
  int width_;
  int height_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_MOCK_PLATFORM_IMAGE_2D_H_
