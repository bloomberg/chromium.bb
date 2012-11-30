// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ASH_FOCUS_RULES_H_
#define ASH_WM_ASH_FOCUS_RULES_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/corewm/focus_rules.h"

namespace ash {
namespace wm {

class ASH_EXPORT AshFocusRules : public views::corewm::FocusRules {
 public:
  AshFocusRules();
  virtual ~AshFocusRules();

 private:
  // Overridden from views::corewm::FocusRules:
  virtual bool CanActivateWindow(aura::Window* window) OVERRIDE;
  virtual bool CanFocusWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetActivatableWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetFocusableWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetNextActivatableWindow(aura::Window* ignore) OVERRIDE;
  virtual aura::Window* GetNextFocusableWindow(aura::Window* ignore) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AshFocusRules);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_ASH_FOCUS_RULES_H_
