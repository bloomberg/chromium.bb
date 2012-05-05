// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "ui/compositor/compositor.h"
#include "ui/surface/transport_dib.h"

namespace gfx {
class Size;
}
class ImageTransportFactory;

// This is a client for ImageTransportSurface, that handles the
// platform-specific task of binding the transport surface to a GL texture.
// The GL texture is allocated in the ImageTransportFactory context, and the
// data is only valid after the first Update().
class ImageTransportClient : public ui::Texture {
 public:
  virtual ~ImageTransportClient() {}

  // Initializes the client with the surface id.
  virtual bool Initialize(uint64* surface_handle) = 0;

  // Updates the surface data into the texture.
  virtual void Update() = 0;

  // Returns the shared memory handle used to transfer software data if needed.
  // Can be a NULL handle.
  virtual TransportDIB::Handle Handle() const = 0;

  // Creates a platform-specific client.
  static ImageTransportClient* Create(ImageTransportFactory* factory,
                                      const gfx::Size& size);

 protected:
  ImageTransportClient(bool flipped, const gfx::Size& size);

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageTransportClient);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_CLIENT_H_

