// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_TREE_HOST_H_
#define COMPONENTS_EXO_SURFACE_TREE_HOST_H_

#include <memory>

#include "base/macros.h"
#include "components/exo/surface.h"
#include "components/exo/surface_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
class WindowDelegate;
}  // namespace aura

namespace gfx {
class Path;
}  // namespace gfx

namespace exo {

// This class provides functionality for hosting a surface tree. The surface
// tree is hosted in the |host_window_|.
class SurfaceTreeHost : public SurfaceDelegate {
 public:
  SurfaceTreeHost(const std::string& window_name,
                  aura::WindowDelegate* window_delegate);
  ~SurfaceTreeHost() override;

  // Sets a root surface of a surface tree. This surface tree will be hosted in
  // the |host_window_|.
  void SetRootSurface(Surface* root_surface);

  // Returns true if hosted surface tree has hittest mask.
  bool HasHitTestMask() const;

  // Returns the hittest mask in |mask| for the hosted surface tree.
  void GetHitTestMask(gfx::Path* mask) const;

  // Returns the bounds of the current input region of the hosted surface tree.
  gfx::Rect GetHitTestBounds() const;

  // Returns the cursor for the given position. If no cursor provider is
  // registered then CursorType::kNull is returned.
  gfx::NativeCursor GetCursor(const gfx::Point& point) const;

  aura::Window* host_window() { return host_window_.get(); }
  const aura::Window* host_window() const { return host_window_.get(); }

  Surface* root_surface() { return root_surface_; }
  const Surface* root_surface() const { return root_surface_; }

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;

 private:
  Surface* root_surface_ = nullptr;
  std::unique_ptr<aura::Window> host_window_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTreeHost);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_TREE_HOST_H_
