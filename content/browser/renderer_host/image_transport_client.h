// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/surface/transport_dib.h"

namespace gfx {
class Size;
}

// This is a client for ImageTransportSurface, that handles the
// platform-specific task of binding the transport surface to a GL texture.
// The GL texture is allocated in the SharedResources context, and the data is
// only valid between Acquire and Release.
class ImageTransportClient {
 public:
  virtual ~ImageTransportClient() {}

  // Initializes the client with the surface id. This returns the GL texture id,
  // or 0 if error.
  virtual unsigned int Initialize(uint64* surface_handle) = 0;

  // Gets the surface data into the texture.
  virtual void Acquire() = 0;

  // Releases the surface data.
  virtual void Release() = 0;

  // Returns whether the data is flipped in the Y direction.
  virtual bool Flipped() = 0;

  // Returns the shared memory handle used to transfer software data if needed.
  // Can be a NULL handle.
  virtual TransportDIB::Handle Handle() const = 0;

  // Creates a platform-specific client.
  static ImageTransportClient* Create(ui::SharedResources* resources,
                                      const gfx::Size& size);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_CLIENT_H_

