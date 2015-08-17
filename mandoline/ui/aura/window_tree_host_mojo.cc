// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/window_tree_host_mojo.h"

#include "components/view_manager/public/cpp/view_manager.h"
#include "mandoline/ui/aura/input_method_mandoline.h"
#include "mandoline/ui/aura/surface_context_factory.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, public:

WindowTreeHostMojo::WindowTreeHostMojo(mojo::Shell* shell, mojo::View* view)
    : view_(view), bounds_(view->bounds().To<gfx::Rect>()) {
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

  input_method_.reset(new InputMethodMandoline(this, view_));
  SetSharedInputMethod(input_method_.get());
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

void WindowTreeHostMojo::ShowImpl() {
  window()->Show();
}

void WindowTreeHostMojo::HideImpl() {
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
// WindowTreeHostMojo, ViewObserver implementation:

void WindowTreeHostMojo::OnViewBoundsChanged(
    mojo::View* view,
    const mojo::Rect& old_bounds,
    const mojo::Rect& new_bounds) {
  gfx::Rect old_bounds2 = old_bounds.To<gfx::Rect>();
  gfx::Rect new_bounds2 = new_bounds.To<gfx::Rect>();
  bounds_ = new_bounds2;
  if (old_bounds2.origin() != new_bounds2.origin())
    OnHostMoved(bounds_.origin());
  if (old_bounds2.size() != new_bounds2.size())
    OnHostResized(bounds_.size());
}

}  // namespace mandoline
