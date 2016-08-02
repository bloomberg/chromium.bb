// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_BACKGROUND_ANIMATOR_OBSERVER_H_
#define ASH_COMMON_SHELF_SHELF_BACKGROUND_ANIMATOR_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

// Observer for the ShelfBackgroundAnimator class.
class ASH_EXPORT ShelfBackgroundAnimatorObserver {
 public:
  // Called when the Shelf's opaque background should be updated.
  virtual void UpdateShelfOpaqueBackground(int alpha) {}

  // Called when the Shelf's asset based background should be updated.
  // TODO(bruthig): Remove when non-md is no longer needed (crbug.com/614453).
  virtual void UpdateShelfAssetBackground(int alpha) {}

  // Called when the Shelf item (aka button) backgrounds should be updated.
  virtual void UpdateShelfItemBackground(int alpha) {}

 protected:
  virtual ~ShelfBackgroundAnimatorObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_BACKGROUND_ANIMATOR_OBSERVER_H_
