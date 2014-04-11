// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_MOJO_H_
#define MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_MOJO_H_

#include "base/bind.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/rect.h"

namespace ui {
class ContextFactory;
}

namespace mojo {
namespace examples {

class WindowTreeHostMojo : public aura::WindowTreeHost,
                           public ui::EventSource,
                           public NativeViewportClient {
 public:
  WindowTreeHostMojo(ScopedNativeViewportHandle viewport_handle,
                     const gfx::Rect& bounds,
                     const base::Callback<void()>& compositor_created_callback);
  virtual ~WindowTreeHostMojo();

  gfx::Rect bounds() const { return bounds_; }
  ScopedMessagePipeHandle TakeGLES2PipeHandle() { return gles2_handle_.Pass(); }

 private:
  // WindowTreeHost:
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void SetCursorNative(gfx::NativeCursor cursor) OVERRIDE;
  virtual void MoveCursorToNative(const gfx::Point& location) OVERRIDE;
  virtual void OnCursorVisibilityChangedNative(bool show) OVERRIDE;

  // ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

  // Overridden from NativeViewportClient:
  virtual void OnCreated() OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;
  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE;
  virtual void OnEvent(const Event& event,
                       const mojo::Callback<void()>& callback) OVERRIDE;

  static ui::ContextFactory* context_factory_;

  ScopedMessagePipeHandle gles2_handle_;
  RemotePtr<NativeViewport> native_viewport_;
  base::Callback<void()> compositor_created_callback_;

  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMojo);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_MOJO_H_
