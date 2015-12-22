// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_LAYOUT_MANAGER_H_
#define MASH_WM_LAYOUT_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"

namespace mash {
namespace wm {

// Base class for container layout managers. Derived classes override
// LayoutWindow() to perform layout and register properties to which
// changes trigger layout.
// LayoutManagers observe both the bound container and all its children.
// They must be attached prior to any windows being added to the
// container.
class LayoutManager : public mus::WindowObserver {
 public:
  ~LayoutManager() override;

 protected:
  explicit LayoutManager(mus::Window* owner);

  void Uninstall();

  // Overridden from mus::WindowObserver:
  void OnTreeChanged(
      const mus::WindowObserver::TreeChangeParams& params) override;
  void OnWindowDestroying(mus::Window* window) override;
  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;

  virtual void WindowAdded(mus::Window* window);
  virtual void WindowRemoved(mus::Window* window);
  virtual void LayoutWindow(mus::Window* window) = 0;

  // Add a property that triggers layout when changed.
  void AddLayoutProperty(const std::string& property);

  mus::Window* owner() { return owner_; }

 private:
  mus::Window* owner_;

  std::set<std::string> layout_properties_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_LAYOUT_MANAGER_H_
