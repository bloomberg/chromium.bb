// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_BRIDGE_MUS_LAYOUT_MANAGER_ADAPTER_H_
#define MASH_WM_BRIDGE_MUS_LAYOUT_MANAGER_ADAPTER_H_

#include <memory>

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"

namespace ash {
namespace wm {
class WmLayoutManager;
}
}

namespace mash {
namespace wm {

// Used to associate a mus::Window with an ash::wm::WmLayoutManager. This
// attaches an observer to the mus::Window and calls the appropriate methods on
// the WmLayoutManager at the appropriate time.
//
// NOTE: WmLayoutManager provides the function SetChildBounds(). This is
// expected to be called to change the bounds of the Window. For aura this
// function is called by way of aura exposing a hook (aura::LayoutManager). Mus
// has no such hook. To ensure SetChildBounds() is called correctly all bounds
// changes to mus::Windows must be routed through WmWindowMus. WmWindowMus
// ensures WmLayoutManager::SetChildBounds() is called appropriately.
class MusLayoutManagerAdapter : public mus::WindowObserver {
 public:
  MusLayoutManagerAdapter(
      mus::Window* window,
      std::unique_ptr<ash::wm::WmLayoutManager> layout_manager);
  ~MusLayoutManagerAdapter() override;

  ash::wm::WmLayoutManager* layout_manager() { return layout_manager_.get(); }

 private:
  // WindowObserver attached to child windows. A separate class is used to
  // easily differentiate WindowObserver calls on the mus::Window associated
  // with the MusLayoutManagerAdapter, vs children.
  class ChildWindowObserver : public mus::WindowObserver {
   public:
    explicit ChildWindowObserver(MusLayoutManagerAdapter* adapter);
    ~ChildWindowObserver() override;

   private:
    // mus::WindowObserver:
    void OnWindowVisibilityChanged(mus::Window* window) override;

    MusLayoutManagerAdapter* adapter_;

    DISALLOW_COPY_AND_ASSIGN(ChildWindowObserver);
  };

  // mus::WindowObserver:
  void OnTreeChanging(const TreeChangeParams& params) override;
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  mus::Window* window_;
  ChildWindowObserver child_window_observer_;
  std::unique_ptr<ash::wm::WmLayoutManager> layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(MusLayoutManagerAdapter);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_BRIDGE_MUS_LAYOUT_MANAGER_ADAPTER_H_
