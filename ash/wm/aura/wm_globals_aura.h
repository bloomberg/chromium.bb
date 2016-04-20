// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_AURA_WM_GLOBALS_AURA_H_
#define ASH_WM_AURA_WM_GLOBALS_AURA_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/wm/common/wm_globals.h"
#include "base/macros.h"

namespace ash {
namespace wm {

class ASH_EXPORT WmGlobalsAura : public WmGlobals {
 public:
  WmGlobalsAura();
  ~WmGlobalsAura() override;

  static WmGlobalsAura* Get();

  // WmGlobals:
  WmWindow* GetActiveWindow() override;
  WmWindow* GetRootWindowForNewWindows() override;
  std::vector<WmWindow*> GetMruWindowListIgnoreModals() override;
  bool IsForceMaximizeOnFirstRun() override;
  std::vector<WmWindow*> GetAllRootWindows() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmGlobalsAura);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_AURA_WM_GLOBALS_AURA_H_
