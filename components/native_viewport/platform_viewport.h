// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_H_
#define COMPONENTS_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_H_

#include "base/memory/scoped_ptr.h"
#include "components/native_viewport/public/interfaces/native_viewport.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace gfx {
class Rect;
}

namespace native_viewport {

// Encapsulation of platform-specific Viewport.
class PlatformViewport {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnMetricsChanged(mojo::ViewportMetricsPtr metrics) = 0;
    virtual void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                              float device_pixel_ratio) = 0;
    virtual void OnAcceleratedWidgetDestroyed() = 0;
    virtual bool OnEvent(mojo::EventPtr event) = 0;
    virtual void OnDestroyed() = 0;
  };

  virtual ~PlatformViewport() {}

  virtual void Init(const gfx::Rect& bounds) = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Close() = 0;
  virtual gfx::Size GetSize() = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  static scoped_ptr<PlatformViewport> Create(Delegate* delegate);
};

}  // namespace native_viewport

#endif  // COMPONENTS_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_H_
