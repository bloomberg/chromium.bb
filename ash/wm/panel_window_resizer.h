// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PANEL_WINDOW_RESIZER_H_
#define ASH_WM_PANEL_WINDOW_RESIZER_H_

#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"

namespace gfx {
class Rect;
}

namespace ash {

namespace internal {
class PanelLayoutManager;
}

// PanelWindowResizer is used by ToplevelWindowEventFilter to handle dragging,
// moving or resizing panel window. These can be attached and detached from the
// launcher.
class ASH_EXPORT PanelWindowResizer : public WindowResizer {
 public:
  virtual ~PanelWindowResizer();

  // Creates a new PanelWindowResizer. The caller takes ownership of the
  // returned object. Returns NULL if not resizable.
  static PanelWindowResizer* Create(aura::Window* window,
                                    const gfx::Point& location,
                                    int window_component);

  // Returns true if the drag will result in changing the window in anyway.
  bool is_resizable() const { return details_.is_resizable; }

  bool changed_size() const {
    return !(details_.bounds_change & kBoundsChange_Repositions);
  }

  // WindowResizer overides:
  virtual void Drag(const gfx::Point& location, int event_flags) OVERRIDE;
  virtual void CompleteDrag(int event_flags) OVERRIDE;
  virtual void RevertDrag() OVERRIDE;
  virtual aura::Window* GetTarget() OVERRIDE;

  const gfx::Point& GetInitialLocationInParentForTest() const {
    return details_.initial_location_in_parent;
  }

 private:
  explicit PanelWindowResizer(const Details& details);

  // Checks if the provided window bounds should attach to the launcher. If true
  // the bounds are modified to snap the window to the launcher.
  bool AttachToLauncher(gfx::Rect* bounds);

  // Tracks the panel's initial position and attachment at the start of a drag
  // and informs the PanelLayoutManager that a drag has started if necessary.
  void StartedDragging();

  // Informs the PanelLayoutManager that the drag is complete if it was informed
  // of the drag start.
  void FinishDragging();

  const Details details_;

  // Panel container window.
  aura::Window* panel_container_;

  // Weak pointer, owned by panel container.
  internal::PanelLayoutManager* panel_layout_manager_;

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_;

  // True if the window started attached to the launcher.
  const bool was_attached_;

  // True if the window should attach to the launcher after releasing.
  bool should_attach_;

  DISALLOW_COPY_AND_ASSIGN(PanelWindowResizer);
};

}  // namespace aura

#endif  // ASH_WM_PANEL_WINDOW_RESIZER_H_
