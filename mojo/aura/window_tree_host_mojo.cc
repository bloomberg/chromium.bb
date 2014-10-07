// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/aura/window_tree_host_mojo.h"

#include "mojo/aura/surface_context_factory.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace mojo {

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, public:

WindowTreeHostMojo::WindowTreeHostMojo(Shell* shell, View* view)
    : view_(view), bounds_(view->bounds()) {
  view_->AddObserver(this);

  context_factory_.reset(new SurfaceContextFactory(shell, view_));
  // WindowTreeHost creates the compositor using the ContextFactory from
  // aura::Env. Install |context_factory_| there so that |context_factory_| is
  // picked up.
  ui::ContextFactory* default_context_factory =
      aura::Env::GetInstance()->context_factory();
  aura::Env::GetInstance()->set_context_factory(context_factory_.get());
  CreateCompositor(GetAcceleratedWidget());
  aura::Env::GetInstance()->set_context_factory(default_context_factory);
  DCHECK_EQ(context_factory_.get(), compositor()->context_factory());
}

WindowTreeHostMojo::~WindowTreeHostMojo() {
  view_->RemoveObserver(this);
  DestroyCompositor();
  DestroyDispatcher();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, aura::WindowTreeHost implementation:

ui::EventSource* WindowTreeHostMojo::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostMojo::GetAcceleratedWidget() {
  return gfx::kNullAcceleratedWidget;
}

void WindowTreeHostMojo::Show() {
  window()->Show();
}

void WindowTreeHostMojo::Hide() {
}

gfx::Rect WindowTreeHostMojo::GetBounds() const {
  return bounds_;
}

void WindowTreeHostMojo::SetBounds(const gfx::Rect& bounds) {
  window()->SetBounds(gfx::Rect(bounds.size()));
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

void WindowTreeHostMojo::PostNativeEvent(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::SetCursorNative(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::MoveCursorToNative(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, ui::EventSource implementation:

ui::EventProcessor* WindowTreeHostMojo::GetEventProcessor() {
  return dispatcher();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, ViewObserver implementation:

void WindowTreeHostMojo::OnViewBoundsChanged(
    View* view,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  bounds_ = new_bounds;
  if (old_bounds.origin() != new_bounds.origin())
    OnHostMoved(bounds_.origin());
  if (old_bounds.size() != new_bounds.size())
    OnHostResized(bounds_.size());
}

}  // namespace mojo
