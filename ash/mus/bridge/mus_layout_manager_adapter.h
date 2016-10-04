// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_MUS_LAYOUT_MANAGER_ADAPTER_H_
#define ASH_MUS_BRIDGE_MUS_LAYOUT_MANAGER_ADAPTER_H_

#include <memory>

#include "base/macros.h"
#include "services/ui/public/cpp/window_observer.h"

namespace ash {
class WmLayoutManager;

namespace mus {

// Used to associate a ui::Window with an WmLayoutManager. This
// attaches an observer to the ui::Window and calls the appropriate methods on
// the WmLayoutManager at the appropriate time.
//
// NOTE: WmLayoutManager provides the function SetChildBounds(). This is
// expected to be called to change the bounds of the Window. For aura this
// function is called by way of aura exposing a hook (aura::LayoutManager). Mus
// has no such hook. To ensure SetChildBounds() is called correctly all bounds
// changes to ui::Windows must be routed through WmWindowMus. WmWindowMus
// ensures WmLayoutManager::SetChildBounds() is called appropriately.
class MusLayoutManagerAdapter : public ui::WindowObserver {
 public:
  MusLayoutManagerAdapter(ui::Window* window,
                          std::unique_ptr<WmLayoutManager> layout_manager);
  ~MusLayoutManagerAdapter() override;

  WmLayoutManager* layout_manager() { return layout_manager_.get(); }

 private:
  // ui::WindowObserver:
  void OnTreeChanging(const TreeChangeParams& params) override;
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnWindowBoundsChanged(ui::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnChildWindowVisibilityChanged(ui::Window* window,
                                      bool visible) override;

  ui::Window* window_;
  std::unique_ptr<WmLayoutManager> layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(MusLayoutManagerAdapter);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_MUS_LAYOUT_MANAGER_ADAPTER_H_
