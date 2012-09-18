// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"

class WebGraphicsContext3DCommandBufferImpl;

namespace content {

class ImageTransportFactoryAndroid {
 public:
  ImageTransportFactoryAndroid();
  ~ImageTransportFactoryAndroid();

  static ImageTransportFactoryAndroid* GetInstance();

  gfx::GLSurfaceHandle CreateSharedSurfaceHandle();
  void DestroySharedSurfaceHandle(const gfx::GLSurfaceHandle& handle);

  uint32_t InsertSyncPoint();

 private:
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_
