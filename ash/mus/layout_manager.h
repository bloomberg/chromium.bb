// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_LAYOUT_MANAGER_H_
#define ASH_MUS_LAYOUT_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"
#include "services/ui/public/cpp/window_observer.h"

namespace ash {
namespace mus {

// Base class for container layout managers. Derived classes override
// LayoutWindow() to perform layout and register properties to which
// changes trigger layout.
// LayoutManagers observe both the bound container and all its children.
// They must be attached prior to any windows being added to the
// container.
class LayoutManager : public ::ui::WindowObserver {
 public:
  ~LayoutManager() override;

 protected:
  explicit LayoutManager(::ui::Window* owner);

  void Uninstall();

  // Overridden from ui::WindowObserver:
  void OnTreeChanged(
      const ::ui::WindowObserver::TreeChangeParams& params) override;
  void OnWindowDestroying(::ui::Window* window) override;
  void OnWindowBoundsChanged(::ui::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowSharedPropertyChanged(
      ::ui::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;

  virtual void WindowAdded(::ui::Window* window);
  virtual void WindowRemoved(::ui::Window* window);
  virtual void LayoutWindow(::ui::Window* window) = 0;

  // Add a property that triggers layout when changed.
  void AddLayoutProperty(const std::string& property);

  ::ui::Window* owner() { return owner_; }

 private:
  ::ui::Window* owner_;

  std::set<std::string> layout_properties_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_LAYOUT_MANAGER_H_
