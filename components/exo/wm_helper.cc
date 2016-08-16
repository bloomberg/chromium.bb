// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper.h"

#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/memory/singleton.h"
#include "ui/aura/client/focus_client.h"
#include "ui/wm/public/activation_client.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// WMHelper, public:

WMHelper::WMHelper() {
  ash::WmShell::Get()->AddShellObserver(this);
  ash::Shell::GetInstance()->activation_client()->AddObserver(this);
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
}

WMHelper::~WMHelper() {
  if (!ash::Shell::HasInstance())
    return;
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
  focus_client->RemoveObserver(this);
  ash::Shell::GetInstance()->activation_client()->RemoveObserver(this);
  ash::WmShell::Get()->RemoveShellObserver(this);
}

// static
WMHelper* WMHelper::GetInstance() {
  return base::Singleton<WMHelper>::get();
}

const ash::DisplayInfo& WMHelper::GetDisplayInfo(int64_t display_id) const {
  return ash::Shell::GetInstance()->display_manager()->GetDisplayInfo(
      display_id);
}

// static
aura::Window* WMHelper::GetContainer(int container_id) {
  return ash::Shell::GetContainer(ash::Shell::GetTargetRootWindow(),
                                  container_id);
}

aura::Window* WMHelper::GetActiveWindow() const {
  return ash::Shell::GetInstance()->activation_client()->GetActiveWindow();
}

void WMHelper::AddActivationObserver(ActivationObserver* observer) {
  activation_observers_.AddObserver(observer);
}

void WMHelper::RemoveActivationObserver(ActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

aura::Window* WMHelper::GetFocusedWindow() const {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  return focus_client->GetFocusedWindow();
}

void WMHelper::AddFocusObserver(FocusObserver* observer) {
  focus_observers_.AddObserver(observer);
}

void WMHelper::RemoveFocusObserver(FocusObserver* observer) {
  focus_observers_.RemoveObserver(observer);
}

ui::CursorSetType WMHelper::GetCursorSet() const {
  return ash::Shell::GetInstance()->cursor_manager()->GetCursorSet();
}

void WMHelper::AddCursorObserver(CursorObserver* observer) {
  cursor_observers_.AddObserver(observer);
}

void WMHelper::RemoveCursorObserver(CursorObserver* observer) {
  cursor_observers_.RemoveObserver(observer);
}

void WMHelper::AddPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->AddPreTargetHandler(handler);
}

void WMHelper::PrependPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->PrependPreTargetHandler(handler);
}

void WMHelper::RemovePreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->RemovePreTargetHandler(handler);
}

void WMHelper::AddPostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->AddPostTargetHandler(handler);
}

void WMHelper::RemovePostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->RemovePostTargetHandler(handler);
}

bool WMHelper::IsMaximizeModeWindowManagerEnabled() const {
  return ash::WmShell::Get()
      ->maximize_mode_controller()
      ->IsMaximizeModeWindowManagerEnabled();
}

void WMHelper::AddMaximizeModeObserver(MaximizeModeObserver* observer) {
  maximize_mode_observers_.AddObserver(observer);
}

void WMHelper::RemoveMaximizeModeObserver(MaximizeModeObserver* observer) {
  maximize_mode_observers_.RemoveObserver(observer);
}

void WMHelper::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  FOR_EACH_OBSERVER(ActivationObserver, activation_observers_,
                    OnWindowActivated(gained_active, lost_active));
}

void WMHelper::OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) {
  FOR_EACH_OBSERVER(FocusObserver, focus_observers_,
                    OnWindowFocused(gained_focus, lost_focus));
}

void WMHelper::OnCursorVisibilityChanged(bool is_visible) {
  FOR_EACH_OBSERVER(CursorObserver, cursor_observers_,
                    OnCursorVisibilityChanged(is_visible));
}

void WMHelper::OnCursorSetChanged(ui::CursorSetType cursor_set) {
  FOR_EACH_OBSERVER(CursorObserver, cursor_observers_,
                    OnCursorSetChanged(cursor_set));
}

void WMHelper::OnMaximizeModeStarted() {
  FOR_EACH_OBSERVER(MaximizeModeObserver, maximize_mode_observers_,
                    OnMaximizeModeStarted());
}

void WMHelper::OnMaximizeModeEnded() {
  FOR_EACH_OBSERVER(MaximizeModeObserver, maximize_mode_observers_,
                    OnMaximizeModeEnded());
}

}  // namespace exo
