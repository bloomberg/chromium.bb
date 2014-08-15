// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gfx {
class Rect;
}

namespace ui {
class Event;
}

namespace mojo {

// Encapsulation of platform-specific Viewport.
class PlatformViewport {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnBoundsChanged(const gfx::Rect& size) = 0;
    virtual void OnAcceleratedWidgetAvailable(
        gfx::AcceleratedWidget widget) = 0;
    virtual bool OnEvent(ui::Event* ui_event) = 0;
    virtual void OnDestroyed() = 0;
  };

  virtual ~PlatformViewport() {}

  virtual void Init(const gfx::Rect& bounds) = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Close() = 0;
  virtual gfx::Size GetSize() = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  virtual void SetCapture() = 0;
  virtual void ReleaseCapture() = 0;

  static scoped_ptr<PlatformViewport> Create(Delegate* delegate);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_H_
