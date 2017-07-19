// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_ASH_H_
#define COMPONENTS_EXO_WM_HELPER_ASH_H_

#include "ash/display/window_tree_host_manager.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace exo {

// A helper class for accessing WindowManager related features.
class WMHelperAsh : public WMHelper,
                    public wm::ActivationChangeObserver,
                    public aura::client::FocusChangeObserver,
                    public aura::client::CursorClientObserver,
                    public ash::ShellObserver,
                    public ash::WindowTreeHostManager::Observer,
                    public ui::InputDeviceEventObserver {
 public:
  WMHelperAsh();
  ~WMHelperAsh() override;

  // Overridden from WMHelper:
  const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const override;
  aura::Window* GetPrimaryDisplayContainer(int container_id) override;
  aura::Window* GetActiveWindow() const override;
  aura::Window* GetFocusedWindow() const override;
  ui::CursorSize GetCursorSize() const override;
  const display::Display& GetCursorDisplay() const override;
  void AddPreTargetHandler(ui::EventHandler* handler) override;
  void PrependPreTargetHandler(ui::EventHandler* handler) override;
  void RemovePreTargetHandler(ui::EventHandler* handler) override;
  void AddPostTargetHandler(ui::EventHandler* handler) override;
  void RemovePostTargetHandler(ui::EventHandler* handler) override;
  bool IsTabletModeWindowManagerEnabled() const override;

  // Overridden from wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from aura::client::CursorClientObserver:
  void OnCursorVisibilityChanged(bool is_visible) override;
  void OnCursorSizeChanged(ui::CursorSize cursor_size) override;
  void OnCursorDisplayChanged(const display::Display& display) override;

  // Overridden from ash::ShellObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnding() override;
  void OnTabletModeEnded() override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // Overridden from ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WMHelperAsh);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
