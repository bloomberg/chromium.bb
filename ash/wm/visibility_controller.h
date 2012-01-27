// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_VISIBILITY_CONTROLLER_H_
#define ASH_WM_VISIBILITY_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/aura/client/visibility_client.h"

namespace ash {
namespace internal {

class VisibilityController : public aura::client::VisibilityClient {
 public:
  VisibilityController();
  virtual ~VisibilityController();

  // Overridden from aura::client::VisibilityClient:
  virtual void UpdateLayerVisibility(aura::Window* window,
                                     bool visible) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(VisibilityController);
};

}  // namespace internal

// Tells |window| to animate visibility changes to its children.
void ASH_EXPORT SetChildWindowVisibilityChangesAnimated(aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_VISIBILITY_CONTROLLER_H_
