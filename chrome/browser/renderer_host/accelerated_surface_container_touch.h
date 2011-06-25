// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
#define CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

// Helper class for storing image data from the GPU process renderered
// on behalf of the RWHVV. It assumes that GL context that will display
// the image data is current  when an instance of this object is created
//  or destroyed.
class AcceleratedSurfaceContainerTouch
    : public base::RefCounted<AcceleratedSurfaceContainerTouch> {
 public:
  explicit AcceleratedSurfaceContainerTouch(uint64 surface_handle);
  uint32 texture() const { return texture_; }
 private:
  friend class base::RefCounted<AcceleratedSurfaceContainerTouch>;

  ~AcceleratedSurfaceContainerTouch();

  void* image_;
  uint32 texture_;
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerTouch);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_ACCELERATED_SURFACE_CONTAINER_TOUCH_H_
