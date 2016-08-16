// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_H_
#define COMPONENTS_EXO_WM_HELPER_H_

#include "ash/common/shell_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
class DisplayInfo;
}

namespace aura {
class Window;
}

namespace ui {
class EventHandler;
}

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace exo {

// A helper class for accessing WindowManager related features.
class WMHelper : public aura::client::ActivationChangeObserver,
                 public aura::client::FocusChangeObserver,
                 public aura::client::CursorClientObserver,
                 public ash::ShellObserver {
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
    virtual void OnCursorSetChanged(ui::CursorSetType cursor_set) {}

   protected:
    virtual ~CursorObserver() {}
  };

  class MaximizeModeObserver {
   public:
    virtual void OnMaximizeModeStarted() = 0;
    virtual void OnMaximizeModeEnded() = 0;

   protected:
    virtual ~MaximizeModeObserver() {}
  };

  static WMHelper* GetInstance();

  const ash::DisplayInfo& GetDisplayInfo(int64_t display_id) const;
  aura::Window* GetContainer(int container_id);

  aura::Window* GetActiveWindow() const;
  void AddActivationObserver(ActivationObserver* observer);
  void RemoveActivationObserver(ActivationObserver* observer);

  aura::Window* GetFocusedWindow() const;
  void AddFocusObserver(FocusObserver* observer);
  void RemoveFocusObserver(FocusObserver* observer);

  ui::CursorSetType GetCursorSet() const;
  void AddCursorObserver(CursorObserver* observer);
  void RemoveCursorObserver(CursorObserver* observer);

  void AddPreTargetHandler(ui::EventHandler* handler);
  void PrependPreTargetHandler(ui::EventHandler* handler);
  void RemovePreTargetHandler(ui::EventHandler* handler);

  void AddPostTargetHandler(ui::EventHandler* handler);
  void RemovePostTargetHandler(ui::EventHandler* handler);

  bool IsMaximizeModeWindowManagerEnabled() const;
  void AddMaximizeModeObserver(MaximizeModeObserver* observer);
  void RemoveMaximizeModeObserver(MaximizeModeObserver* observer);

 private:
  WMHelper();
  ~WMHelper() override;

  friend struct base::DefaultSingletonTraits<WMHelper>;

  // Overriden from aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Overriden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overriden from aura::client::CursorClientObserver:
  void OnCursorVisibilityChanged(bool is_visible) override;
  void OnCursorSetChanged(ui::CursorSetType cursor_set) override;

  // Overriden from ash::ShellObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

  base::ObserverList<ActivationObserver> activation_observers_;
  base::ObserverList<FocusObserver> focus_observers_;
  base::ObserverList<CursorObserver> cursor_observers_;
  base::ObserverList<MaximizeModeObserver> maximize_mode_observers_;

  DISALLOW_COPY_AND_ASSIGN(WMHelper);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
