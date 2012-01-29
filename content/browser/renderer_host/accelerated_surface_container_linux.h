// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_LINUX_H_
#define CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_LINUX_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/surface/transport_dib.h"

class ImageTransportClient;

// Helper class for storing image data from the GPU process renderered
// on behalf of the RWHVV. It assumes that GL context that will display
// the image data is current  when an instance of this object is created
// or destroyed.
class AcceleratedSurfaceContainerLinux : public ui::Texture {
 public:
  explicit AcceleratedSurfaceContainerLinux(const gfx::Size& size);
  virtual ~AcceleratedSurfaceContainerLinux();

  // Initialize the surface container, and returns an ID for it.
  // The |surface_handle| given to this function may be modified, and the
  // modified value should be used to identify the object.
  bool Initialize(uint64* surface_handle);

  // Some implementations of this class use shared memory, this is the handle
  // to the shared buffer, which is part of the surface container.
  TransportDIB::Handle Handle() const;

  void Update();

 private:
  scoped_ptr<ImageTransportClient> image_transport_client_;
  bool acquired_;
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerLinux);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_LINUX_H_
