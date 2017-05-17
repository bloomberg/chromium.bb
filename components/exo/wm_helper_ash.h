// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_ASH_H_
#define COMPONENTS_EXO_WM_HELPER_ASH_H_

#include "ash/shell_observer.h"
#include "ash/system/accessibility_observer.h"
#include "ash/wm_display_observer.h"
#include "base/macros.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace exo {

// A helper class for accessing WindowManager related features.
class WMHelperAsh : public WMHelper,
                    public aura::client::ActivationChangeObserver,
                    public aura::client::FocusChangeObserver,
                    public aura::client::CursorClientObserver,
                    public ash::AccessibilityObserver,
                    public ash::ShellObserver,
                    public ash::WmDisplayObserver,
                    public ui::InputDeviceEventObserver {
 public:
  WMHelperAsh();
  ~WMHelperAsh() override;

  // Overridden from WMHelper:
  const display::ManagedDisplayInfo GetDisplayInfo(
      int64_t display_id) const override;
  aura::Window* GetContainer(int container_id) override;
  aura::Window* GetActiveWindow() const override;
  aura::Window* GetFocusedWindow() const override;
  ui::CursorSetType GetCursorSet() const override;
  void AddPreTargetHandler(ui::EventHandler* handler) override;
  void PrependPreTargetHandler(ui::EventHandler* handler) override;
  void RemovePreTargetHandler(ui::EventHandler* handler) override;
  void AddPostTargetHandler(ui::EventHandler* handler) override;
  void RemovePostTargetHandler(ui::EventHandler* handler) override;
  bool IsMaximizeModeWindowManagerEnabled() const override;
  bool IsSpokenFeedbackEnabled() const override;
  void PlayEarcon(int sound_key) const override;

  // Overridden from aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from aura::client::CursorClientObserver:
  void OnCursorVisibilityChanged(bool is_visible) override;
  void OnCursorSetChanged(ui::CursorSetType cursor_set) override;

  // Overridden from ash::AccessibilityObserver:
  void OnAccessibilityModeChanged(
      ash::AccessibilityNotificationVisibility notify) override;

  // Overridden from ash::ShellObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnding() override;
  void OnMaximizeModeEnded() override;

  // Overridden from ash::WmDisplayObserver:
  void OnDisplayConfigurationChanged() override;

  // Overridden from ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WMHelperAsh);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
