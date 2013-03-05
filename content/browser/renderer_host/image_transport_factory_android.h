// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class GLShareGroup;
}

namespace WebKit {
class WebGraphicsContext3D;
}

namespace content {
class GLHelper;

class ImageTransportFactoryAndroid {
 public:
  virtual ~ImageTransportFactoryAndroid();

  static ImageTransportFactoryAndroid* GetInstance();

  virtual uint32_t InsertSyncPoint() = 0;
  virtual void WaitSyncPoint(uint32_t sync_point) = 0;
  virtual uint32_t CreateTexture() = 0;
  virtual void DeleteTexture(uint32_t id) = 0;
  virtual void AcquireTexture(
      uint32 texture_id, const signed char* mailbox_name) = 0;
  virtual void ReleaseTexture(
      uint32 texture_id, const signed char* mailbox_name) = 0;

  virtual WebKit::WebGraphicsContext3D* GetContext3D() = 0;
  virtual GLHelper* GetGLHelper() = 0;

protected:
  ImageTransportFactoryAndroid();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_
