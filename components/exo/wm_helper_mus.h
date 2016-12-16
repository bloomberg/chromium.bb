// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_MUS_H_
#define COMPONENTS_EXO_WM_HELPER_MUS_H_

#include "base/macros.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace aura {
namespace client {
class ActivationClient;
}
}

namespace exo {

// A helper class for accessing WindowManager related features.
class WMHelperMus : public WMHelper,
                    public ui::InputDeviceEventObserver,
                    public aura::EnvObserver,
                    public aura::client::FocusChangeObserver {
 public:
  WMHelperMus();
  ~WMHelperMus() override;

  // Overriden from WMHelper:
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

  // Overriden from aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;
  void OnActiveFocusClientChanged(aura::client::FocusClient* focus_client,
                                  aura::Window* window) override;

  // Overriden from ui::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overriden from ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;

 private:
  void SetActiveFocusClient(aura::client::FocusClient* focus_client,
                            aura::Window* window);
  void SetActiveWindow(aura::Window* window);
  void SetFocusedWindow(aura::Window* window);

  aura::client::ActivationClient* GetActivationClient();

  // Current FocusClient.
  aura::client::FocusClient* active_focus_client_ = nullptr;
  // This is the window that |active_focus_client_| comes from (may be null).
  aura::Window* root_with_active_focus_client_ = nullptr;
  aura::Window* active_window_ = nullptr;
  aura::Window* focused_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WMHelperMus);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
