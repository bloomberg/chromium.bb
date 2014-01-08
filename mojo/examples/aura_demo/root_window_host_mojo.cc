// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/aura_demo/root_window_host_mojo.h"

#include "mojo/examples/aura_demo/demo_context_factory.h"
#include "mojo/examples/compositor_app/gles2_client_impl.h"
#include "mojo/public/gles2/gles2.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_delegate.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {
namespace examples {

// static
ui::ContextFactory* WindowTreeHostMojo::context_factory_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, public:

WindowTreeHostMojo::WindowTreeHostMojo(
    ScopedMessagePipeHandle viewport_handle,
    const base::Callback<void()>& compositor_created_callback)
    : native_viewport_(viewport_handle.Pass(), this),
      compositor_created_callback_(compositor_created_callback) {
  native_viewport_->Open();

  ScopedMessagePipeHandle gles2_handle;
  ScopedMessagePipeHandle gles2_client_handle;
  CreateMessagePipe(&gles2_handle, &gles2_client_handle);

  // The ContextFactory must exist before any Compositors are created.
  if (!context_factory_) {
    scoped_ptr<DemoContextFactory> cf(new DemoContextFactory(this));
    if (cf->Initialize())
      context_factory_ = cf.release();
    ui::ContextFactory::SetInstance(context_factory_);
  }
  CHECK(context_factory_) << "No GL bindings.";

  gles2_client_.reset(new GLES2ClientImpl(
      gles2_handle.Pass(),
      base::Bind(&WindowTreeHostMojo::DidCreateContext,
                 base::Unretained(this))));
  native_viewport_->CreateGLES2Context(gles2_client_handle.Pass());
}

WindowTreeHostMojo::~WindowTreeHostMojo() {}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, aura::WindowTreeHost implementation:

aura::RootWindow* WindowTreeHostMojo::GetRootWindow() {
  return delegate_->AsRootWindow();
}

gfx::AcceleratedWidget WindowTreeHostMojo::GetAcceleratedWidget() {
  NOTIMPLEMENTED();
  return gfx::kNullAcceleratedWidget;
}

void WindowTreeHostMojo::Show() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::Hide() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::ToggleFullScreen() {
  NOTIMPLEMENTED();
}

gfx::Rect WindowTreeHostMojo::GetBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect(500, 500);
}

void WindowTreeHostMojo::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Insets WindowTreeHostMojo::GetInsets() const {
  NOTIMPLEMENTED();
  return gfx::Insets();
}

void WindowTreeHostMojo::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED();
}

gfx::Point WindowTreeHostMojo::GetLocationOnNativeScreen() const {
  return gfx::Point(0, 0);
}

void WindowTreeHostMojo::SetCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::SetCursor(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

bool WindowTreeHostMojo::QueryMouseLocation(gfx::Point* location_return) {
  NOTIMPLEMENTED();
  return false;
}

bool WindowTreeHostMojo::ConfineCursorToRootWindow() {
  NOTIMPLEMENTED();
  return false;
}

void WindowTreeHostMojo::UnConfineCursor() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::OnCursorVisibilityChanged(bool show) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::PostNativeEvent(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::OnDeviceScaleFactorChanged(float device_scale_factor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, NativeViewportClient implementation:

void WindowTreeHostMojo::OnCreated() {
}

void WindowTreeHostMojo::OnDestroyed() {
  base::MessageLoop::current()->Quit();
}

void WindowTreeHostMojo::OnEvent(const Event& event) {
  if (!event.location().is_null())
    native_viewport_->AckEvent(event);

  // TODO(beng): fwd to rootwindow.
};

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, private:

void WindowTreeHostMojo::DidCreateContext(gfx::Size viewport_size) {
  CreateCompositor(GetAcceleratedWidget());
  compositor_created_callback_.Run();
  NotifyHostResized(viewport_size);
}

}  // namespace examples
}  // namespace mojo
