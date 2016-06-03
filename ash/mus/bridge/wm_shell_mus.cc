// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_shell_mus.h"

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/window_resizer.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/container_ids.h"
#include "ash/mus/drag_window_resizer.h"
#include "ash/mus/root_window_controller.h"
#include "base/memory/ptr_util.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"

namespace ash {
namespace mus {

WmShellMus::WmShellMus(::mus::WindowTreeClient* client) : client_(client) {
  client_->AddObserver(this);
  WmShell::Set(this);
}

WmShellMus::~WmShellMus() {
  RemoveClientObserver();
  WmShell::Set(nullptr);
}

// static
WmShellMus* WmShellMus::Get() {
  return static_cast<WmShellMus*>(WmShell::Get());
}

void WmShellMus::AddRootWindowController(
    WmRootWindowControllerMus* controller) {
  root_window_controllers_.push_back(controller);
}

void WmShellMus::RemoveRootWindowController(
    WmRootWindowControllerMus* controller) {
  auto iter = std::find(root_window_controllers_.begin(),
                        root_window_controllers_.end(), controller);
  DCHECK(iter != root_window_controllers_.end());
  root_window_controllers_.erase(iter);
}

// static
WmWindowMus* WmShellMus::GetToplevelAncestor(::mus::Window* window) {
  while (window) {
    if (IsActivationParent(window->parent()))
      return WmWindowMus::Get(window);
    window = window->parent();
  }
  return nullptr;
}

WmRootWindowControllerMus* WmShellMus::GetRootWindowControllerWithDisplayId(
    int64_t id) {
  for (WmRootWindowControllerMus* root_window_controller :
       root_window_controllers_) {
    if (root_window_controller->GetDisplay().id() == id)
      return root_window_controller;
  }
  NOTREACHED();
  return nullptr;
}

WmWindow* WmShellMus::NewContainerWindow() {
  return WmWindowMus::Get(client_->NewWindow());
}

WmWindow* WmShellMus::GetFocusedWindow() {
  return WmWindowMus::Get(client_->GetFocusedWindow());
}

WmWindow* WmShellMus::GetActiveWindow() {
  return GetToplevelAncestor(client_->GetFocusedWindow());
}

WmWindow* WmShellMus::GetPrimaryRootWindow() {
  return root_window_controllers_[0]->GetWindow();
}

WmWindow* WmShellMus::GetRootWindowForDisplayId(int64_t display_id) {
  return GetRootWindowControllerWithDisplayId(display_id)->GetWindow();
}

WmWindow* WmShellMus::GetRootWindowForNewWindows() {
  NOTIMPLEMENTED();
  return root_window_controllers_[0]->GetWindow();
}

std::vector<WmWindow*> WmShellMus::GetMruWindowList() {
  NOTIMPLEMENTED();
  return std::vector<WmWindow*>();
}

std::vector<WmWindow*> WmShellMus::GetMruWindowListIgnoreModals() {
  NOTIMPLEMENTED();
  return std::vector<WmWindow*>();
}

bool WmShellMus::IsForceMaximizeOnFirstRun() {
  NOTIMPLEMENTED();
  return false;
}

bool WmShellMus::IsUserSessionBlocked() {
  NOTIMPLEMENTED();
  return false;
}

bool WmShellMus::IsScreenLocked() {
  NOTIMPLEMENTED();
  return false;
}

void WmShellMus::LockCursor() {
  NOTIMPLEMENTED();
}

void WmShellMus::UnlockCursor() {
  NOTIMPLEMENTED();
}

std::vector<WmWindow*> WmShellMus::GetAllRootWindows() {
  std::vector<WmWindow*> wm_windows(root_window_controllers_.size());
  for (size_t i = 0; i < root_window_controllers_.size(); ++i)
    wm_windows[i] = root_window_controllers_[i]->GetWindow();
  return wm_windows;
}

void WmShellMus::RecordUserMetricsAction(wm::WmUserMetricsAction action) {
  NOTIMPLEMENTED();
}

std::unique_ptr<WindowResizer> WmShellMus::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::WrapUnique(
      new DragWindowResizer(std::move(next_window_resizer), window_state));
}

bool WmShellMus::IsOverviewModeSelecting() {
  NOTIMPLEMENTED();
  return false;
}

bool WmShellMus::IsOverviewModeRestoringMinimizedWindows() {
  NOTIMPLEMENTED();
  return false;
}

void WmShellMus::AddActivationObserver(WmActivationObserver* observer) {
  activation_observers_.AddObserver(observer);
}

void WmShellMus::RemoveActivationObserver(WmActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

void WmShellMus::AddDisplayObserver(WmDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void WmShellMus::RemoveDisplayObserver(WmDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void WmShellMus::AddOverviewModeObserver(WmOverviewModeObserver* observer) {
  NOTIMPLEMENTED();
}

void WmShellMus::RemoveOverviewModeObserver(WmOverviewModeObserver* observer) {
  NOTIMPLEMENTED();
}

// static
bool WmShellMus::IsActivationParent(::mus::Window* window) {
  if (!window)
    return false;

  for (size_t i = 0; i < kNumActivationContainers; ++i) {
    if (window->local_id() == static_cast<int>(kActivationContainers[i]))
      return true;
  }
  return false;
}

void WmShellMus::RemoveClientObserver() {
  if (!client_)
    return;

  client_->RemoveObserver(this);
  client_ = nullptr;
}

// TODO: support OnAttemptToReactivateWindow, http://crbug.com/615114.
void WmShellMus::OnWindowTreeFocusChanged(::mus::Window* gained_focus,
                                          ::mus::Window* lost_focus) {
  WmWindowMus* gained_active = GetToplevelAncestor(gained_focus);
  WmWindowMus* lost_active = GetToplevelAncestor(gained_focus);
  if (gained_active == lost_active)
    return;

  FOR_EACH_OBSERVER(WmActivationObserver, activation_observers_,
                    OnWindowActivated(gained_active, lost_active));
}

void WmShellMus::OnWillDestroyClient(::mus::WindowTreeClient* client) {
  DCHECK_EQ(client, client_);
  RemoveClientObserver();
}

}  // namespace mus
}  // namespace ash
