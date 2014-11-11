// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_UTIL_FILL_LAYOUT_MANAGER_H_
#define ATHENA_UTIL_FILL_LAYOUT_MANAGER_H_

#include "athena/athena_export.h"
#include "ui/aura/layout_manager.h"

namespace athena {

class ATHENA_EXPORT FillLayoutManager : public aura::LayoutManager {
 public:
  static void SetAlwaysFill(aura::Window* window);

  explicit FillLayoutManager(aura::Window* container);
  ~FillLayoutManager() override;

  // aura::LayoutManager:
  virtual void OnWindowResized() override;
  virtual void OnWindowAddedToLayout(aura::Window* child) override;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) override;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) override;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) override;

 private:
  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(FillLayoutManager);
};

}  // namespace athena

#endif  // ATHENA_UTIL_FILL_LAYOUT_MANAGER_H_
