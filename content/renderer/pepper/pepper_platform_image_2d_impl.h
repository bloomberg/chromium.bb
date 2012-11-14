// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_IMAGE_2D_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_IMAGE_2D_IMPL_H_

#include "base/basictypes.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace content {

// Implements the Image2D using a TransportDIB.
class PepperPlatformImage2DImpl
    : public webkit::ppapi::PluginDelegate::PlatformImage2D {
 public:
  virtual ~PepperPlatformImage2DImpl();

  // Factory function that properly allocates a TransportDIB according to
  // the platform. Returns NULL on failure.
  static PepperPlatformImage2DImpl* Create(int width, int height);

  // PlatformImage2D implementation.
  virtual SkCanvas* Map() OVERRIDE;
  virtual intptr_t GetSharedMemoryHandle(uint32* byte_count) const OVERRIDE;
  virtual TransportDIB* GetTransportDIB() const OVERRIDE;

 private:
  // This constructor will take ownership of the dib pointer.
  // On Mac, we assume that the dib is cached by the browser, so on destruction
  // we'll tell the browser to free it.
  PepperPlatformImage2DImpl(int width, int height, TransportDIB* dib);

  int width_;
  int height_;
  scoped_ptr<TransportDIB> dib_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformImage2DImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_IMAGE_2D_IMPL_H_
