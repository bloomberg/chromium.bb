// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_IO_SURFACE_CONTEXT_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_IO_SURFACE_CONTEXT_MAC_H_

#include <OpenGL/OpenGL.h>
#include <map>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "ui/gl/scoped_cgl.h"

namespace content {

// The CGL context used to update and draw an IOSurface-backed texture. If an
// error occurs on this context, or if the current GPU changes, then the
// context is poisoned, and a new one is created.
class IOSurfaceContext
    : public base::RefCounted<IOSurfaceContext>,
      public content::GpuDataManagerObserver {
 public:
  // Get or create a GL context. Share these GL contexts as much as possible
  // because creating and destroying them can be expensive.
  static scoped_refptr<IOSurfaceContext> Get();

  // Mark that all the GL contexts in the same sharegroup as this context as
  // invalid, so they shouldn't be returned anymore by Get, but rather, new
  // contexts should be created. This is called as a precaution when unexpected
  // GL errors occur, or after a GPU switch.
  void PoisonContextAndSharegroup();
  bool HasBeenPoisoned() const { return poisoned_; }

  CGLContextObj cgl_context() const { return cgl_context_; }

  // content::GpuDataManagerObserver implementation.
  virtual void OnGpuSwitching() override;

 private:
  friend class base::RefCounted<IOSurfaceContext>;

  IOSurfaceContext(base::ScopedTypeRef<CGLContextObj> clg_context_strong);
  virtual ~IOSurfaceContext();

  base::ScopedTypeRef<CGLContextObj> cgl_context_;
  bool poisoned_;

  // The current non-poisoned context, shared by all layers.
  static IOSurfaceContext* current_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_IO_SURFACE_CONTEXT_MAC_H_
