// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_FOCUS_MANAGER_FACTORY_H_
#define ASH_ACCELERATORS_FOCUS_MANAGER_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/focus/focus_manager_delegate.h"
#include "ui/views/focus/focus_manager_factory.h"

namespace ash {

// A factory class for creating a custom views::FocusManager object which
// supports Ash shortcuts.
class AshFocusManagerFactory : public views::FocusManagerFactory {
 public:
  AshFocusManagerFactory();
  virtual ~AshFocusManagerFactory();

 protected:
  // views::FocusManagerFactory overrides:
  virtual views::FocusManager* CreateFocusManager(
      views::Widget* widget) OVERRIDE;

 private:
  class Delegate : public views::FocusManagerDelegate {
   public:
    // views::FocusManagerDelegate overrides:
    virtual bool ProcessAccelerator(
        const ui::Accelerator& accelerator) OVERRIDE;
    virtual ui::AcceleratorTarget* GetCurrentTargetForAccelerator(
        const ui::Accelerator& accelerator) const OVERRIDE;
  };

  DISALLOW_COPY_AND_ASSIGN(AshFocusManagerFactory);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_FOCUS_MANAGER_FACTORY_H_
