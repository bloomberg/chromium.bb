// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_AURA_AURA_LAYOUT_MANAGER_ADAPTER_H_
#define ASH_WM_AURA_AURA_LAYOUT_MANAGER_ADAPTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/layout_manager.h"

namespace ash {
namespace wm {

class WmLayoutManager;

// AuraLayoutManagerAdapter is an aura::LayoutManager that calls to
// WmLayoutManager. Use it when you have a WmLayoutManager you want to use
// with an aura::Window.
class ASH_EXPORT AuraLayoutManagerAdapter : public aura::LayoutManager {
 public:
  explicit AuraLayoutManagerAdapter(
      std::unique_ptr<WmLayoutManager> wm_layout_manager);
  ~AuraLayoutManagerAdapter() override;

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
  std::unique_ptr<WmLayoutManager> wm_layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(AuraLayoutManagerAdapter);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_AURA_AURA_LAYOUT_MANAGER_ADAPTER_H_
