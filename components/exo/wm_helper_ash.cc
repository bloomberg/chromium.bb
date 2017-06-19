// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_ash.h"

#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/memory/singleton.h"
#include "ui/aura/client/focus_client.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/wm/public/activation_client.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// WMHelperAsh, public:

WMHelperAsh::WMHelperAsh() {
  ash::Shell::Get()->AddShellObserver(this);
  ash::Shell::Get()->activation_client()->AddObserver(this);
  // TODO(crbug.com/631103): Mushrome doesn't have a cursor manager yet.
  if (ash::Shell::GetAshConfig() != ash::Config::MUS)
    ash::Shell::Get()->cursor_manager()->AddObserver(this);
  ash::Shell::Get()->window_tree_host_manager()->AddObserver(this);
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

WMHelperAsh::~WMHelperAsh() {
  if (!ash::Shell::HasInstance())
    return;
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->RemoveObserver(this);
  ash::Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  // TODO(crbug.com/631103): Mushrome doesn't have a cursor manager yet.
  if (ash::Shell::GetAshConfig() != ash::Config::MUS)
    ash::Shell::Get()->cursor_manager()->RemoveObserver(this);
  ash::Shell::Get()->activation_client()->RemoveObserver(this);
  ash::Shell::Get()->RemoveShellObserver(this);
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WMHelperAsh, private:

const display::ManagedDisplayInfo& WMHelperAsh::GetDisplayInfo(
    int64_t display_id) const {
  return ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
}

aura::Window* WMHelperAsh::GetPrimaryDisplayContainer(int container_id) {
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

aura::Window* WMHelperAsh::GetActiveWindow() const {
  return ash::Shell::Get()->activation_client()->GetActiveWindow();
}

aura::Window* WMHelperAsh::GetFocusedWindow() const {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  return focus_client->GetFocusedWindow();
}

ui::CursorSetType WMHelperAsh::GetCursorSet() const {
  // TODO(crbug.com/631103): Mushrome doesn't have a cursor manager yet.
  if (ash::Shell::GetAshConfig() == ash::Config::MUS)
    return ui::CURSOR_SET_NORMAL;
  return ash::Shell::Get()->cursor_manager()->GetCursorSet();
}

const display::Display& WMHelperAsh::GetCursorDisplay() const {
  // TODO(crbug.com/631103): Mushrome doesn't have a cursor manager yet.
  if (ash::Shell::GetAshConfig() == ash::Config::MUS) {
    static const display::Display display;
    return display;
  }
  return ash::Shell::Get()->cursor_manager()->GetDisplay();
}

void WMHelperAsh::AddPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->AddPreTargetHandler(handler);
}

void WMHelperAsh::PrependPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->PrependPreTargetHandler(handler);
}

void WMHelperAsh::RemovePreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->RemovePreTargetHandler(handler);
}

void WMHelperAsh::AddPostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->AddPostTargetHandler(handler);
}

void WMHelperAsh::RemovePostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->RemovePostTargetHandler(handler);
}

bool WMHelperAsh::IsMaximizeModeWindowManagerEnabled() const {
  return ash::Shell::Get()
      ->maximize_mode_controller()
      ->IsMaximizeModeWindowManagerEnabled();
}

void WMHelperAsh::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  NotifyWindowActivated(gained_active, lost_active);
}

void WMHelperAsh::OnWindowFocused(aura::Window* gained_focus,
                                  aura::Window* lost_focus) {
  NotifyWindowFocused(gained_focus, lost_focus);
}

void WMHelperAsh::OnCursorVisibilityChanged(bool is_visible) {
  NotifyCursorVisibilityChanged(is_visible);
}

void WMHelperAsh::OnCursorSetChanged(ui::CursorSetType cursor_set) {
  NotifyCursorSetChanged(cursor_set);
}

void WMHelperAsh::OnCursorDisplayChanged(const display::Display& display) {
  NotifyCursorDisplayChanged(display);
}

void WMHelperAsh::OnMaximizeModeStarted() {
  NotifyMaximizeModeStarted();
}

void WMHelperAsh::OnMaximizeModeEnding() {
  NotifyMaximizeModeEnding();
}

void WMHelperAsh::OnMaximizeModeEnded() {
  NotifyMaximizeModeEnded();
}

void WMHelperAsh::OnDisplayConfigurationChanged() {
  NotifyDisplayConfigurationChanged();
}

void WMHelperAsh::OnKeyboardDeviceConfigurationChanged() {
  NotifyKeyboardDeviceConfigurationChanged();
}

}  // namespace exo
