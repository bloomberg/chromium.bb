// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_modality_controller.h"

#include <algorithm>

#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {

namespace wm {

namespace {

bool TransientChildIsWindowModal(aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_WINDOW;
}

aura::Window* GetWindowModalTransientChild(aura::Window* window) {
  aura::Window::Windows::const_iterator it;
  for (it = window->transient_children().begin();
       it != window->transient_children().end();
       ++it) {
    if (TransientChildIsWindowModal(*it) && (*it)->IsVisible()) {
      if (!(*it)->transient_children().empty())
        return GetWindowModalTransientChild(*it);
      return *it;
    }
  }
  return NULL;
}

}  // namespace

aura::Window* GetWindowModalTransient(aura::Window* window) {
  if (!window)
    return NULL;

  // We always want to check the for the transient child of the activatable
  // window.
  window = wm::GetActivatableWindow(window);
  if (!window)
    return NULL;

  return GetWindowModalTransientChild(window);
}

}  // namespace wm

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// WindowModalityController, public:

WindowModalityController::WindowModalityController() {
  aura::Env::GetInstance()->AddObserver(this);
}

WindowModalityController::~WindowModalityController() {
  aura::Env::GetInstance()->RemoveObserver(this);
  for (size_t i = 0; i < windows_.size(); ++i)
    windows_[i]->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WindowModalityController, aura::EventFilter implementation:

bool WindowModalityController::PreHandleKeyEvent(aura::Window* target,
                                                 aura::KeyEvent* event) {
  return !!wm::GetWindowModalTransient(target);
}

bool WindowModalityController::PreHandleMouseEvent(aura::Window* target,
                                                   aura::MouseEvent* event) {
  aura::Window* modal_transient_child = wm::GetWindowModalTransient(target);
  if (modal_transient_child && event->type() == ui::ET_MOUSE_PRESSED)
    wm::ActivateWindow(modal_transient_child);
  return !!modal_transient_child;
}

ui::TouchStatus WindowModalityController::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  // TODO: make touch work with modals.
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus WindowModalityController::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  // TODO: make gestures work with modals.
  return ui::GESTURE_STATUS_UNKNOWN;
}

void WindowModalityController::OnWindowInitialized(aura::Window* window) {
  windows_.push_back(window);
  window->AddObserver(this);
}

void WindowModalityController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (visible && window->GetProperty(aura::client::kModalKey) ==
      ui::MODAL_TYPE_WINDOW) {
    // Make sure no other window has capture, otherwise |window| won't get mouse
    // events.
    aura::Window* capture_window = aura::client::GetCaptureWindow(window);
    if (capture_window)
      capture_window->ReleaseCapture();
  }
}

void WindowModalityController::OnWindowDestroyed(aura::Window* window) {
  windows_.erase(std::find(windows_.begin(), windows_.end(), window));
  window->RemoveObserver(this);
}

}  // namespace internal
}  // namespace ash
