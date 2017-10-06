// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_ash.h"

#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/singleton.h"
#include "ui/aura/client/focus_client.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/wm/public/activation_client.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// WMHelperAsh, public:

WMHelperAsh::WMHelperAsh() {
  ash::Shell::Get()->tablet_mode_controller()->AddObserver(this);
  ash::Shell::Get()->activation_client()->AddObserver(this);
  // TODO(crbug.com/631103): Mushrome doesn't have a cursor manager yet.
  if (ash::Shell::GetAshConfig() == ash::Config::CLASSIC)
    ash::Shell::Get()->cursor_manager()->AddObserver(this);
  ash::Shell::Get()->window_tree_host_manager()->AddObserver(this);
  aura::Window* primary_root = ash::Shell::GetPrimaryRootWindow();
  aura::client::GetFocusClient(primary_root)->AddObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
  // TODO(reveman): Multi-display support.
  vsync_manager_ = primary_root->layer()->GetCompositor()->vsync_manager();
  vsync_manager_->AddObserver(this);
}

WMHelperAsh::~WMHelperAsh() {
  if (!ash::Shell::HasInstance())
    return;
  vsync_manager_->RemoveObserver(this);
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->RemoveObserver(this);
  ash::Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  // TODO(crbug.com/631103): Mushrome doesn't have a cursor manager yet.
  if (ash::Shell::GetAshConfig() != ash::Config::MUS)
    ash::Shell::Get()->cursor_manager()->RemoveObserver(this);
  ash::Shell::Get()->activation_client()->RemoveObserver(this);
  ash::Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
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

ui::CursorSize WMHelperAsh::GetCursorSize() const {
  // TODO(crbug.com/631103): Mushrome doesn't have a cursor manager yet.
  if (ash::Shell::GetAshConfig() == ash::Config::MUS)
    return ui::CursorSize::kNormal;
  return ash::Shell::Get()->cursor_manager()->GetCursorSize();
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

bool WMHelperAsh::IsTabletModeWindowManagerEnabled() const {
  return ash::Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

double WMHelperAsh::GetDefaultDeviceScaleFactor() const {
  if (!display::Display::HasInternalDisplay())
    return 1.0;

  if (display::Display::HasForceDeviceScaleFactor())
    return display::Display::GetForcedDeviceScaleFactor();

  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(display::Display::InternalDisplayId());
  for (auto& mode : display_info.display_modes()) {
    if (mode->is_default())
      return mode->device_scale_factor();
  }

  NOTREACHED();
  return 1.0f;
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

void WMHelperAsh::OnCursorSizeChanged(ui::CursorSize cursor_size) {
  NotifyCursorSizeChanged(cursor_size);
}

void WMHelperAsh::OnCursorDisplayChanged(const display::Display& display) {
  NotifyCursorDisplayChanged(display);
}

void WMHelperAsh::OnTabletModeStarted() {
  NotifyTabletModeStarted();
}

void WMHelperAsh::OnTabletModeEnding() {
  NotifyTabletModeEnding();
}

void WMHelperAsh::OnTabletModeEnded() {
  NotifyTabletModeEnded();
}

void WMHelperAsh::OnDisplayConfigurationChanged() {
  NotifyDisplayConfigurationChanged();
}

void WMHelperAsh::OnKeyboardDeviceConfigurationChanged() {
  NotifyKeyboardDeviceConfigurationChanged();
}

void WMHelperAsh::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                          base::TimeDelta interval) {
  NotifyUpdateVSyncParameters(timebase, interval);
}

}  // namespace exo
