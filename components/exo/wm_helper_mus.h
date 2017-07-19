// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_MUS_H_
#define COMPONENTS_EXO_WM_HELPER_MUS_H_

#include "base/macros.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/mus/focus_synchronizer_observer.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace wm {
class ActivationClient;
}

namespace exo {

// A helper class for accessing WindowManager related features.
// This is only used for mash. Mushrome uses WMHelperAsh.
class WMHelperMus : public WMHelper,
                    public ui::InputDeviceEventObserver,
                    public aura::FocusSynchronizerObserver,
                    public aura::client::FocusChangeObserver {
 public:
  WMHelperMus();
  ~WMHelperMus() override;

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

  // Overridden from aura::FocusSynchronizerObserver:
  void OnActiveFocusClientChanged(aura::client::FocusClient* focus_client,
                                  aura::Window* focus_client_root) override;

  // Overridden from ui::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;

 private:
  void SetActiveFocusClient(aura::client::FocusClient* focus_client,
                            aura::Window* window);
  void SetActiveWindow(aura::Window* window);
  void SetFocusedWindow(aura::Window* window);

  wm::ActivationClient* GetActivationClient();

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
