// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/backdrop_delegate.h"

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// A backdrop delegate for MaximizedMode, which always creates a backdrop.
// This is also used in the WorkspaceLayoutManagerBackdropTest, hence
// is public.
class ASH_EXPORT MaximizeModeBackdropDelegateImpl : public BackdropDelegate {
 public:
  MaximizeModeBackdropDelegateImpl();
  ~MaximizeModeBackdropDelegateImpl() override;

 protected:
  bool HasBackdrop(aura::Window* window) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MaximizeModeBackdropDelegateImpl);
};

}  // namespace ash
