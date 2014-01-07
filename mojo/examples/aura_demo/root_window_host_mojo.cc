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
ui::ContextFactory* RootWindowHostMojo::context_factory_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// RootWindowHostMojo, public:

RootWindowHostMojo::RootWindowHostMojo(
    ScopedMessagePipeHandle viewport_handle,
    const base::Callback<void()>& compositor_created_callback)
    : native_viewport_(viewport_handle.Pass()),
      compositor_created_callback_(compositor_created_callback) {
  native_viewport_.SetPeer(this);
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
      base::Bind(&RootWindowHostMojo::DidCreateContext,
                 base::Unretained(this))));
  native_viewport_->CreateGLES2Context(gles2_client_handle.Pass());
}

RootWindowHostMojo::~RootWindowHostMojo() {}

////////////////////////////////////////////////////////////////////////////////
// RootWindowHostMojo, aura::RootWindowHost implementation:

aura::RootWindow* RootWindowHostMojo::GetRootWindow() {
  return delegate_->AsRootWindow();
}

gfx::AcceleratedWidget RootWindowHostMojo::GetAcceleratedWidget() {
  NOTIMPLEMENTED();
  return gfx::kNullAcceleratedWidget;
}

void RootWindowHostMojo::Show() {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::Hide() {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::ToggleFullScreen() {
  NOTIMPLEMENTED();
}

gfx::Rect RootWindowHostMojo::GetBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect(500, 500);
}

void RootWindowHostMojo::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Insets RootWindowHostMojo::GetInsets() const {
  NOTIMPLEMENTED();
  return gfx::Insets();
}

void RootWindowHostMojo::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED();
}

gfx::Point RootWindowHostMojo::GetLocationOnNativeScreen() const {
  return gfx::Point(0, 0);
}

void RootWindowHostMojo::SetCapture() {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::SetCursor(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

bool RootWindowHostMojo::QueryMouseLocation(gfx::Point* location_return) {
  NOTIMPLEMENTED();
  return false;
}

bool RootWindowHostMojo::ConfineCursorToRootWindow() {
  NOTIMPLEMENTED();
  return false;
}

void RootWindowHostMojo::UnConfineCursor() {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::OnCursorVisibilityChanged(bool show) {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::PostNativeEvent(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::OnDeviceScaleFactorChanged(float device_scale_factor) {
  NOTIMPLEMENTED();
}

void RootWindowHostMojo::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowHostMojo, NativeViewportClientStub implementation:

void RootWindowHostMojo::OnCreated() {
}

void RootWindowHostMojo::OnDestroyed() {
  base::MessageLoop::current()->Quit();
}

void RootWindowHostMojo::OnEvent(const Event& event) {
  if (!event.location().is_null())
    native_viewport_->AckEvent(event);

  // TODO(beng): fwd to rootwindow.
};

////////////////////////////////////////////////////////////////////////////////
// RootWindowHostMojo, private:

void RootWindowHostMojo::DidCreateContext(gfx::Size viewport_size) {
  CreateCompositor(GetAcceleratedWidget());
  compositor_created_callback_.Run();
  NotifyHostResized(viewport_size);
}

}  // namespace examples
}  // namespace mojo
