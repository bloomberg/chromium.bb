// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
#define CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/compositor/compositor_gl.h"

// Helper class for storing image data from the GPU process renderered
// on behalf of the RWHVV. It assumes that GL context that will display
// the image data is current  when an instance of this object is created
// or destroyed.
class AcceleratedSurfaceContainerTouch : public ui::TextureGL {
 public:
  static AcceleratedSurfaceContainerTouch* CreateAcceleratedSurfaceContainer(
      const gfx::Size& size,
      uint64 surface_handle);

  // TextureGL implementation
  virtual void SetCanvas(const SkCanvas& canvas,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE;

 protected:
  explicit AcceleratedSurfaceContainerTouch(const gfx::Size& size);

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerTouch);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
