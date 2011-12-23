// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_STACKING_CONTROLLER_H_
#define ASH_WM_STACKING_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/stacking_client.h"

namespace aura_shell {
namespace internal {

class AlwaysOnTopController;

class StackingController : public aura::client::StackingClient {
 public:
  StackingController();
  virtual ~StackingController();

  // Overridden from aura::client::StackingClient:
  virtual aura::Window* GetDefaultParent(aura::Window* window) OVERRIDE;

 private:
  // Returns corresponding modal container for a modal window.
  // If screen lock is not active, all modal windows are placed into the
  // normal modal container.
  // Otherwise those that originate from LockScreen container and above are
  // placed in the screen lock modal container.
  aura::Window* GetModalContainer(aura::Window* window) const;

  scoped_ptr<internal::AlwaysOnTopController> always_on_top_controller_;

  DISALLOW_COPY_AND_ASSIGN(StackingController);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // ASH_WM_STACKING_CONTROLLER_H_
