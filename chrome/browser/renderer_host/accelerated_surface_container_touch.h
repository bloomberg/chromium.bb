// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
#define CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/compositor/compositor_gl.h"
#include "ui/gfx/surface/transport_dib.h"

// Helper class for storing image data from the GPU process renderered
// on behalf of the RWHVV. It assumes that GL context that will display
// the image data is current  when an instance of this object is created
// or destroyed.
class AcceleratedSurfaceContainerTouch : public ui::TextureGL {
 public:
  static AcceleratedSurfaceContainerTouch* CreateAcceleratedSurfaceContainer(
      const gfx::Size& size);

  // TextureGL implementation
  virtual void SetCanvas(const SkCanvas& canvas,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE;

  // Initialize the surface container, and returns an ID for it.
  // The |surface_id| given to this function may be modified, and the modified
  // value should be used to identify the object.
  virtual bool Initialize(uint64* surface_id) = 0;

  // Some implementations of this class use shared memory, this is the handle
  // to the shared buffer, which is part of the surface container.
  virtual TransportDIB::Handle Handle() const;

 protected:
  explicit AcceleratedSurfaceContainerTouch(const gfx::Size& size);

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerTouch);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
