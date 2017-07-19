// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_H_
#define COMPONENTS_EXO_WM_HELPER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/cursor/cursor.h"

namespace aura {
class Window;
}

namespace display {
class Display;
class ManagedDisplayInfo;
}

namespace ui {
class EventHandler;
}

namespace exo {

// A helper class for accessing WindowManager related features.
class WMHelper {
 public:
  class ActivationObserver {
   public:
    virtual void OnWindowActivated(aura::Window* gained_active,
                                   aura::Window* lost_active) = 0;

   protected:
    virtual ~ActivationObserver() {}
  };

  class FocusObserver {
   public:
    virtual void OnWindowFocused(aura::Window* gained_focus,
                                 aura::Window* lost_focus) = 0;

   protected:
    virtual ~FocusObserver() {}
  };

  class CursorObserver {
   public:
    virtual void OnCursorVisibilityChanged(bool is_visible) {}
    virtual void OnCursorSizeChanged(ui::CursorSize cursor_size) {}
    virtual void OnCursorDisplayChanged(const display::Display& display) {}

   protected:
    virtual ~CursorObserver() {}
  };

  class TabletModeObserver {
   public:
    virtual void OnTabletModeStarted() = 0;
    virtual void OnTabletModeEnding() = 0;
    virtual void OnTabletModeEnded() = 0;

   protected:
    virtual ~TabletModeObserver() {}
  };

  class InputDeviceEventObserver {
   public:
    virtual void OnKeyboardDeviceConfigurationChanged() = 0;

   protected:
    virtual ~InputDeviceEventObserver() {}
  };

  class DisplayConfigurationObserver {
   public:
    virtual void OnDisplayConfigurationChanged() = 0;

   protected:
    virtual ~DisplayConfigurationObserver() {}
  };

  virtual ~WMHelper();

  static void SetInstance(WMHelper* helper);
  static WMHelper* GetInstance();

  void AddActivationObserver(ActivationObserver* observer);
  void RemoveActivationObserver(ActivationObserver* observer);
  void AddFocusObserver(FocusObserver* observer);
  void RemoveFocusObserver(FocusObserver* observer);
  void AddCursorObserver(CursorObserver* observer);
  void RemoveCursorObserver(CursorObserver* observer);
  void AddTabletModeObserver(TabletModeObserver* observer);
  void RemoveTabletModeObserver(TabletModeObserver* observer);
  void AddInputDeviceEventObserver(InputDeviceEventObserver* observer);
  void RemoveInputDeviceEventObserver(InputDeviceEventObserver* observer);
  void AddDisplayConfigurationObserver(DisplayConfigurationObserver* observer);
  void RemoveDisplayConfigurationObserver(
      DisplayConfigurationObserver* observer);

  virtual const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const = 0;
  virtual aura::Window* GetPrimaryDisplayContainer(int container_id) = 0;
  virtual aura::Window* GetActiveWindow() const = 0;
  virtual aura::Window* GetFocusedWindow() const = 0;
  virtual ui::CursorSize GetCursorSize() const = 0;
  virtual const display::Display& GetCursorDisplay() const = 0;
  virtual void AddPreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void PrependPreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void RemovePreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void AddPostTargetHandler(ui::EventHandler* handler) = 0;
  virtual void RemovePostTargetHandler(ui::EventHandler* handler) = 0;
  virtual bool IsTabletModeWindowManagerEnabled() const = 0;

 protected:
  WMHelper();

  void NotifyWindowActivated(aura::Window* gained_active,
                             aura::Window* lost_active);
  void NotifyWindowFocused(aura::Window* gained_focus,
                           aura::Window* lost_focus);
  void NotifyCursorVisibilityChanged(bool is_visible);
  void NotifyCursorSizeChanged(ui::CursorSize cursor_size);
  void NotifyCursorDisplayChanged(const display::Display& display);
  void NotifyTabletModeStarted();
  void NotifyTabletModeEnding();
  void NotifyTabletModeEnded();
  void NotifyKeyboardDeviceConfigurationChanged();
  void NotifyDisplayConfigurationChanged();

 private:
  base::ObserverList<ActivationObserver> activation_observers_;
  base::ObserverList<FocusObserver> focus_observers_;
  base::ObserverList<CursorObserver> cursor_observers_;
  base::ObserverList<TabletModeObserver> tablet_mode_observers_;
  base::ObserverList<InputDeviceEventObserver> input_device_event_observers_;
  base::ObserverList<DisplayConfigurationObserver> display_config_observers_;

  DISALLOW_COPY_AND_ASSIGN(WMHelper);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
