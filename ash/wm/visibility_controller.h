// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_VISIBILITY_CONTROLLER_H_
#define ASH_WM_VISIBILITY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/aura/client/visibility_client.h"

namespace ash {
namespace internal {

class ASH_EXPORT VisibilityController : public aura::client::VisibilityClient {
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

// Suspends the animations for visibility changes during the lifetime of an
// instance of this class.
//
// Example:
//
// void ViewName::UnanimatedAction() {
//   SuspendChildWindowVisibilityAnimations suspend(parent);
//   // Perform unanimated action here.
//   // ...
//   // When the method finishes, visibility animations will return to their
//   // previous state.
// }
//
class ASH_EXPORT SuspendChildWindowVisibilityAnimations {
 public:
  // Suspend visibility animations of child windows.
  explicit SuspendChildWindowVisibilityAnimations(aura::Window* window);

  // Restore visibility animations to their original state.
  ~SuspendChildWindowVisibilityAnimations();

 private:
  // The window to manage.
  aura::Window* window_;

  // Whether the visibility animations on child windows were originally enabled.
  const bool original_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SuspendChildWindowVisibilityAnimations);
};

// Tells |window| to animate visibility changes to its children.
void ASH_EXPORT SetChildWindowVisibilityChangesAnimated(aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_VISIBILITY_CONTROLLER_H_
