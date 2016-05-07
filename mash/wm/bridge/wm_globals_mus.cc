// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/bridge/wm_globals_mus.h"

#include "ash/wm/common/window_resizer.h"
#include "ash/wm/common/wm_activation_observer.h"
#include "ash/wm/common/wm_shell_window_ids.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mash/wm/bridge/wm_root_window_controller_mus.h"
#include "mash/wm/bridge/wm_window_mus.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mash/wm/root_window_controller.h"

namespace mash {
namespace wm {

WmGlobalsMus::WmGlobalsMus(mus::WindowTreeConnection* connection)
    : connection_(connection) {
  connection_->AddObserver(this);
  WmGlobals::Set(this);
}

WmGlobalsMus::~WmGlobalsMus() {
  connection_->RemoveObserver(this);
  WmGlobals::Set(nullptr);
}

// static
WmGlobalsMus* WmGlobalsMus::Get() {
  return static_cast<WmGlobalsMus*>(ash::wm::WmGlobals::Get());
}

void WmGlobalsMus::AddRootWindowController(
    WmRootWindowControllerMus* controller) {
  root_window_controllers_.push_back(controller);
}

void WmGlobalsMus::RemoveRootWindowController(
    WmRootWindowControllerMus* controller) {
  auto iter = std::find(root_window_controllers_.begin(),
                        root_window_controllers_.end(), controller);
  DCHECK(iter != root_window_controllers_.end());
  root_window_controllers_.erase(iter);
}

// static
WmWindowMus* WmGlobalsMus::GetToplevelAncestor(mus::Window* window) {
  while (window) {
    if (IsActivationParent(window->parent()))
      return WmWindowMus::Get(window);
    window = window->parent();
  }
  return nullptr;
}

WmRootWindowControllerMus* WmGlobalsMus::GetRootWindowControllerWithDisplayId(
    int64_t id) {
  for (WmRootWindowControllerMus* root_window_controller :
       root_window_controllers_) {
    if (root_window_controller->GetDisplay().id() == id)
      return root_window_controller;
  }
  NOTREACHED();
  return nullptr;
}

ash::wm::WmWindow* WmGlobalsMus::GetFocusedWindow() {
  return WmWindowMus::Get(connection_->GetFocusedWindow());
}

ash::wm::WmWindow* WmGlobalsMus::GetActiveWindow() {
  return GetToplevelAncestor(connection_->GetFocusedWindow());
}

ash::wm::WmWindow* WmGlobalsMus::GetRootWindowForDisplayId(int64_t display_id) {
  return GetRootWindowControllerWithDisplayId(display_id)->GetWindow();
}

ash::wm::WmWindow* WmGlobalsMus::GetRootWindowForNewWindows() {
  NOTIMPLEMENTED();
  return root_window_controllers_[0]->GetWindow();
}

std::vector<ash::wm::WmWindow*> WmGlobalsMus::GetMruWindowListIgnoreModals() {
  NOTIMPLEMENTED();
  return std::vector<ash::wm::WmWindow*>();
}

bool WmGlobalsMus::IsForceMaximizeOnFirstRun() {
  NOTIMPLEMENTED();
  return false;
}

bool WmGlobalsMus::IsUserSessionBlocked() {
  NOTIMPLEMENTED();
  return false;
}

bool WmGlobalsMus::IsScreenLocked() {
  NOTIMPLEMENTED();
  return false;
}

void WmGlobalsMus::LockCursor() {
  NOTIMPLEMENTED();
}

void WmGlobalsMus::UnlockCursor() {
  NOTIMPLEMENTED();
}

std::vector<ash::wm::WmWindow*> WmGlobalsMus::GetAllRootWindows() {
  std::vector<ash::wm::WmWindow*> wm_windows(root_window_controllers_.size());
  for (size_t i = 0; i < root_window_controllers_.size(); ++i)
    wm_windows[i] = root_window_controllers_[i]->GetWindow();
  return wm_windows;
}

void WmGlobalsMus::RecordUserMetricsAction(
    ash::wm::WmUserMetricsAction action) {
  NOTIMPLEMENTED();
}

std::unique_ptr<ash::WindowResizer> WmGlobalsMus::CreateDragWindowResizer(
    std::unique_ptr<ash::WindowResizer> next_window_resizer,
    ash::wm::WindowState* window_state) {
  NOTIMPLEMENTED();
  return next_window_resizer;
}

bool WmGlobalsMus::IsOverviewModeSelecting() {
  NOTIMPLEMENTED();
  return false;
}

bool WmGlobalsMus::IsOverviewModeRestoringMinimizedWindows() {
  NOTIMPLEMENTED();
  return false;
}

void WmGlobalsMus::AddActivationObserver(
    ash::wm::WmActivationObserver* observer) {
  activation_observers_.AddObserver(observer);
}

void WmGlobalsMus::RemoveActivationObserver(
    ash::wm::WmActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

void WmGlobalsMus::AddDisplayObserver(ash::wm::WmDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void WmGlobalsMus::RemoveDisplayObserver(ash::wm::WmDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void WmGlobalsMus::AddOverviewModeObserver(
    ash::wm::WmOverviewModeObserver* observer) {
  NOTIMPLEMENTED();
}

void WmGlobalsMus::RemoveOverviewModeObserver(
    ash::wm::WmOverviewModeObserver* observer) {
  NOTIMPLEMENTED();
}

// static
bool WmGlobalsMus::IsActivationParent(mus::Window* window) {
  return window && window->local_id() == ash::kShellWindowId_DefaultContainer;
}

void WmGlobalsMus::OnWindowTreeFocusChanged(mus::Window* gained_focus,
                                            mus::Window* lost_focus) {
  WmWindowMus* gained_active = GetToplevelAncestor(gained_focus);
  WmWindowMus* lost_active = GetToplevelAncestor(gained_focus);
  if (gained_active == lost_active)
    return;

  FOR_EACH_OBSERVER(ash::wm::WmActivationObserver, activation_observers_,
                    OnWindowActivated(gained_active, lost_active));
}

}  // namespace wm
}  // namespace mash
