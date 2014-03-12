// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ASH_FOCUS_RULES_H_
#define ASH_WM_ASH_FOCUS_RULES_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/wm/core/base_focus_rules.h"

namespace ash {
namespace wm {

class ASH_EXPORT AshFocusRules : public ::wm::BaseFocusRules {
 public:
  AshFocusRules();
  virtual ~AshFocusRules();

 private:
  // Overridden from ::wm::BaseFocusRules:
  virtual bool SupportsChildActivation(aura::Window* window) const OVERRIDE;
  virtual bool IsWindowConsideredVisibleForActivation(
      aura::Window* window) const OVERRIDE;
  virtual bool CanActivateWindow(aura::Window* window) const OVERRIDE;
  virtual aura::Window* GetNextActivatableWindow(
      aura::Window* ignore) const OVERRIDE;

  aura::Window* GetTopmostWindowToActivateForContainerIndex(
      int index,
      aura::Window* ignore) const;
  aura::Window* GetTopmostWindowToActivateInContainer(
      aura::Window* container,
      aura::Window* ignore) const;

  DISALLOW_COPY_AND_ASSIGN(AshFocusRules);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_ASH_FOCUS_RULES_H_
