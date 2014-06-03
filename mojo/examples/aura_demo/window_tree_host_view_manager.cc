// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/aura_demo/window_tree_host_view_manager.h"

#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace mojo {
namespace examples {

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostViewManager, public:

WindowTreeHostViewManager::WindowTreeHostViewManager(const gfx::Rect& bounds)
    : bounds_(bounds) {
  CreateCompositor(GetAcceleratedWidget());
}

WindowTreeHostViewManager::~WindowTreeHostViewManager() {
  DestroyCompositor();
  DestroyDispatcher();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostViewManager, aura::WindowTreeHost implementation:

ui::EventSource* WindowTreeHostViewManager::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostViewManager::GetAcceleratedWidget() {
  NOTIMPLEMENTED() << "GetAcceleratedWidget";
  return gfx::kNullAcceleratedWidget;
}

void WindowTreeHostViewManager::Show() {
  window()->Show();
}

void WindowTreeHostViewManager::Hide() {
}

gfx::Rect WindowTreeHostViewManager::GetBounds() const {
  return bounds_;
}

void WindowTreeHostViewManager::SetBounds(const gfx::Rect& bounds) {
}

gfx::Point WindowTreeHostViewManager::GetLocationOnNativeScreen() const {
  return gfx::Point(0, 0);
}

void WindowTreeHostViewManager::SetCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostViewManager::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostViewManager::PostNativeEvent(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

void WindowTreeHostViewManager::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostViewManager::SetCursorNative(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostViewManager::MoveCursorToNative(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WindowTreeHostViewManager::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostViewManager, ui::EventSource implementation:

ui::EventProcessor* WindowTreeHostViewManager::GetEventProcessor() {
  return dispatcher();
}

}  // namespace examples
}  // namespace mojo
