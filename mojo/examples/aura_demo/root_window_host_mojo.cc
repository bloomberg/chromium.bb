// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/aura_demo/root_window_host_mojo.h"

#include "mojo/examples/aura_demo/demo_context_factory.h"
#include "mojo/examples/compositor_app/gles2_client_impl.h"
#include "mojo/public/gles2/gles2.h"
#include "mojo/services/native_viewport/geometry_conversions.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_delegate.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
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
    const gfx::Rect& bounds,
    const base::Callback<void()>& compositor_created_callback)
    : native_viewport_(viewport_handle.Pass(), this),
      compositor_created_callback_(compositor_created_callback) {
  AllocationScope scope;
  native_viewport_->Create(bounds);

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
  NOTIMPLEMENTED() << "GetAcceleratedWidget";
  return gfx::kNullAcceleratedWidget;
}

void WindowTreeHostMojo::Show() {
  native_viewport_->Show();
}

void WindowTreeHostMojo::Hide() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::ToggleFullScreen() {
  NOTIMPLEMENTED();
}

gfx::Rect WindowTreeHostMojo::GetBounds() const {
  return bounds_;
}

void WindowTreeHostMojo::SetBounds(const gfx::Rect& bounds) {
  AllocationScope scope;
  native_viewport_->SetBounds(bounds);
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
  NOTIMPLEMENTED() << "QueryMouseLocation";
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

void WindowTreeHostMojo::OnBoundsChanged(const Rect& bounds) {
  bounds_ = gfx::Rect(bounds.position().x(), bounds.position().y(),
                      bounds.size().width(), bounds.size().height());
  window()->SetBounds(gfx::Rect(bounds_.size()));
}

void WindowTreeHostMojo::OnDestroyed() {
  base::MessageLoop::current()->Quit();
}

void WindowTreeHostMojo::OnEvent(const Event& event) {
  if (!event.location().is_null())
    native_viewport_->AckEvent(event);

  switch (event.action()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED: {
      gfx::Point location(event.location().x(), event.location().y());
      ui::MouseEvent ev(static_cast<ui::EventType>(event.action()), location,
                        location, event.flags(), 0);
      delegate_->OnHostMouseEvent(&ev);
      break;
    }
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED: {
      ui::KeyEvent ev(
          static_cast<ui::EventType>(event.action()),
          static_cast<ui::KeyboardCode>(event.key_data().key_code()),
          event.flags(), event.key_data().is_char());
      delegate_->OnHostKeyEvent(&ev);
      break;
    }
    // TODO(beng): touch, etc.
  }
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
