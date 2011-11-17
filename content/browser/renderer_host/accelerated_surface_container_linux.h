// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_LINUX_H_
#define CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_LINUX_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/surface/transport_dib.h"

// Helper class for storing image data from the GPU process renderered
// on behalf of the RWHVV. It assumes that GL context that will display
// the image data is current  when an instance of this object is created
// or destroyed.
class CONTENT_EXPORT AcceleratedSurfaceContainerLinux {
 public:
  virtual ~AcceleratedSurfaceContainerLinux() { }
  virtual void AddRef() = 0;
  virtual void Release() = 0;

  // Initialize the surface container, and returns an ID for it.
  // The |surface_id| given to this function may be modified, and the modified
  // value should be used to identify the object.
  virtual bool Initialize(uint64* surface_id) = 0;

  // Some implementations of this class use shared memory, this is the handle
  // to the shared buffer, which is part of the surface container.
  virtual TransportDIB::Handle Handle() const = 0;

  virtual ui::Texture* GetTexture() = 0;

  static AcceleratedSurfaceContainerLinux* Create(const gfx::Size& size);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_LINUX_H_
