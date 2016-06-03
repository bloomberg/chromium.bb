// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_globals_mus.h"

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/window_resizer.h"
#include "ash/common/wm/wm_activation_observer.h"
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

WmGlobalsMus::WmGlobalsMus(::mus::WindowTreeClient* client) : client_(client) {
  client_->AddObserver(this);
  WmGlobals::Set(this);
}

WmGlobalsMus::~WmGlobalsMus() {
  RemoveClientObserver();
  WmGlobals::Set(nullptr);
}

// static
WmGlobalsMus* WmGlobalsMus::Get() {
  return static_cast<WmGlobalsMus*>(wm::WmGlobals::Get());
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
WmWindowMus* WmGlobalsMus::GetToplevelAncestor(::mus::Window* window) {
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

wm::WmWindow* WmGlobalsMus::NewContainerWindow() {
  return WmWindowMus::Get(client_->NewWindow());
}

wm::WmWindow* WmGlobalsMus::GetFocusedWindow() {
  return WmWindowMus::Get(client_->GetFocusedWindow());
}

wm::WmWindow* WmGlobalsMus::GetActiveWindow() {
  return GetToplevelAncestor(client_->GetFocusedWindow());
}

wm::WmWindow* WmGlobalsMus::GetPrimaryRootWindow() {
  return root_window_controllers_[0]->GetWindow();
}

wm::WmWindow* WmGlobalsMus::GetRootWindowForDisplayId(int64_t display_id) {
  return GetRootWindowControllerWithDisplayId(display_id)->GetWindow();
}

wm::WmWindow* WmGlobalsMus::GetRootWindowForNewWindows() {
  NOTIMPLEMENTED();
  return root_window_controllers_[0]->GetWindow();
}

std::vector<wm::WmWindow*> WmGlobalsMus::GetMruWindowList() {
  NOTIMPLEMENTED();
  return std::vector<wm::WmWindow*>();
}

std::vector<wm::WmWindow*> WmGlobalsMus::GetMruWindowListIgnoreModals() {
  NOTIMPLEMENTED();
  return std::vector<wm::WmWindow*>();
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

std::vector<wm::WmWindow*> WmGlobalsMus::GetAllRootWindows() {
  std::vector<wm::WmWindow*> wm_windows(root_window_controllers_.size());
  for (size_t i = 0; i < root_window_controllers_.size(); ++i)
    wm_windows[i] = root_window_controllers_[i]->GetWindow();
  return wm_windows;
}

void WmGlobalsMus::RecordUserMetricsAction(wm::WmUserMetricsAction action) {
  NOTIMPLEMENTED();
}

std::unique_ptr<WindowResizer> WmGlobalsMus::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::WrapUnique(
      new DragWindowResizer(std::move(next_window_resizer), window_state));
}

bool WmGlobalsMus::IsOverviewModeSelecting() {
  NOTIMPLEMENTED();
  return false;
}

bool WmGlobalsMus::IsOverviewModeRestoringMinimizedWindows() {
  NOTIMPLEMENTED();
  return false;
}

void WmGlobalsMus::AddActivationObserver(wm::WmActivationObserver* observer) {
  activation_observers_.AddObserver(observer);
}

void WmGlobalsMus::RemoveActivationObserver(
    wm::WmActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

void WmGlobalsMus::AddDisplayObserver(wm::WmDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void WmGlobalsMus::RemoveDisplayObserver(wm::WmDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void WmGlobalsMus::AddOverviewModeObserver(
    wm::WmOverviewModeObserver* observer) {
  NOTIMPLEMENTED();
}

void WmGlobalsMus::RemoveOverviewModeObserver(
    wm::WmOverviewModeObserver* observer) {
  NOTIMPLEMENTED();
}

// static
bool WmGlobalsMus::IsActivationParent(::mus::Window* window) {
  if (!window)
    return false;

  for (size_t i = 0; i < kNumActivationContainers; ++i) {
    if (window->local_id() == static_cast<int>(kActivationContainers[i]))
      return true;
  }
  return false;
}

void WmGlobalsMus::RemoveClientObserver() {
  if (!client_)
    return;

  client_->RemoveObserver(this);
  client_ = nullptr;
}

// TODO: support OnAttemptToReactivateWindow, http://crbug.com/615114.
void WmGlobalsMus::OnWindowTreeFocusChanged(::mus::Window* gained_focus,
                                            ::mus::Window* lost_focus) {
  WmWindowMus* gained_active = GetToplevelAncestor(gained_focus);
  WmWindowMus* lost_active = GetToplevelAncestor(gained_focus);
  if (gained_active == lost_active)
    return;

  FOR_EACH_OBSERVER(wm::WmActivationObserver, activation_observers_,
                    OnWindowActivated(gained_active, lost_active));
}

void WmGlobalsMus::OnWillDestroyClient(::mus::WindowTreeClient* client) {
  DCHECK_EQ(client, client_);
  RemoveClientObserver();
}

}  // namespace mus
}  // namespace ash
