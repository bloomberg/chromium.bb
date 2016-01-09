// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_SURFACE_LOOKUP_H_
#define CONTENT_COMMON_GPU_GPU_SURFACE_LOOKUP_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "ui/gl/android/scoped_java_surface.h"
#endif

namespace content {

// This class provides an interface to look up window surface handles
// that cannot be sent through the IPC channel.
class CONTENT_EXPORT GpuSurfaceLookup {
 public:
  GpuSurfaceLookup() { }
  virtual ~GpuSurfaceLookup() { }

  static GpuSurfaceLookup* GetInstance();
  static void InitInstance(GpuSurfaceLookup* lookup);

  virtual gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) = 0;

#if defined(OS_ANDROID)
  virtual gfx::ScopedJavaSurface AcquireJavaSurface(int surface_id);
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuSurfaceLookup);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_SURFACE_LOOKUP_H_
