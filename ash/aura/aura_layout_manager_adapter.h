// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_AURA_LAYOUT_MANAGER_ADAPTER_H_
#define ASH_AURA_AURA_LAYOUT_MANAGER_ADAPTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/layout_manager.h"

namespace ash {

class WmLayoutManager;

// AuraLayoutManagerAdapter is an aura::LayoutManager that calls to
// WmLayoutManager. Use it when you have a WmLayoutManager you want to use
// with an aura::Window.
class ASH_EXPORT AuraLayoutManagerAdapter : public aura::LayoutManager {
 public:
  AuraLayoutManagerAdapter(aura::Window* window,
                           std::unique_ptr<WmLayoutManager> wm_layout_manager);
  ~AuraLayoutManagerAdapter() override;

  // Returns the AuraLayoutManagerAdapter installed on |window|, or null if
  // an AuraLayoutManagerAdapter is not installed on |window|.
  static AuraLayoutManagerAdapter* Get(aura::Window* window);

  WmLayoutManager* wm_layout_manager() { return wm_layout_manager_.get(); }

  // aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  aura::Window* window_;
  std::unique_ptr<WmLayoutManager> wm_layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(AuraLayoutManagerAdapter);
};

}  // namespace ash

#endif  // ASH_AURA_AURA_LAYOUT_MANAGER_ADAPTER_H_
