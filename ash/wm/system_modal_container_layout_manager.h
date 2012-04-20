// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
#pragma once

#include <vector>

#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ash/ash_export.h"

namespace aura {
class Window;
class EventFilter;
}
namespace gfx {
class Rect;
}
namespace views {
class Widget;
}

namespace ash {
namespace internal {

// LayoutManager for the modal window container.
class ASH_EXPORT SystemModalContainerLayoutManager
    : public aura::LayoutManager,
      public aura::WindowObserver,
      public SystemModalContainerEventFilterDelegate {
 public:
  explicit SystemModalContainerLayoutManager(aura::Window* container);
  virtual ~SystemModalContainerLayoutManager();

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overridden from SystemModalContainerEventFilterDelegate:
  virtual bool CanWindowReceiveEvents(aura::Window* window) OVERRIDE;

 private:
  void AddModalWindow(aura::Window* window);
  void RemoveModalWindow(aura::Window* window);

  void CreateModalScreen();
  void DestroyModalScreen();

  aura::Window* modal_window() {
    return !modal_windows_.empty() ? modal_windows_.back() : NULL;
  }

  // The container that owns the layout manager.
  aura::Window* container_;

  // A "screen" widget that dims the windows behind the modal window(s) being
  // shown in |container_|.
  views::Widget* modal_screen_;

  // A stack of modal windows. Only the topmost can receive events.
  std::vector<aura::Window*> modal_windows_;

  // An event filter that enforces the modality of the topmost window in
  // |modal_windows_|. The event filter is attached when a modal window is
  // added, and removed when the last is closed.
  scoped_ptr<aura::EventFilter> modality_filter_;

  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
