// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_AURA_WM_GLOBALS_AURA_H_
#define ASH_WM_AURA_WM_GLOBALS_AURA_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/wm/common/wm_globals.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
namespace wm {

class ASH_EXPORT WmGlobalsAura : public WmGlobals,
                                 public aura::client::ActivationChangeObserver {
 public:
  WmGlobalsAura();
  ~WmGlobalsAura() override;

  static WmGlobalsAura* Get();

  // WmGlobals:
  WmWindow* GetFocusedWindow() override;
  WmWindow* GetActiveWindow() override;
  WmWindow* GetRootWindowForDisplayId(int64_t display_id) override;
  WmWindow* GetRootWindowForNewWindows() override;
  std::vector<WmWindow*> GetMruWindowListIgnoreModals() override;
  bool IsForceMaximizeOnFirstRun() override;
  void LockCursor() override;
  void UnlockCursor() override;
  std::vector<WmWindow*> GetAllRootWindows() override;
  UserMetricsRecorder* GetUserMetricsRecorder() override;
  void AddActivationObserver(WmActivationObserver* observer) override;
  void RemoveActivationObserver(WmActivationObserver* observer) override;

 private:
  // aura::client::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  bool added_activation_observer_ = false;
  base::ObserverList<WmActivationObserver> activation_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmGlobalsAura);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_AURA_WM_GLOBALS_AURA_H_
