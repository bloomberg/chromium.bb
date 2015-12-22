// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_FRAME_MOVE_LOOP_H_
#define MASH_WM_FRAME_MOVE_LOOP_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/interfaces/input_events.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace mash {
namespace wm {

// MoveLoop is responsible for moving/resizing windows.
class MoveLoop : public mus::WindowObserver {
 public:
  enum MoveResult {
    // The move is still ongoing.
    CONTINUE,

    // The move is done and the MoveLoop should be destroyed.
    DONE,
  };

  enum class HorizontalLocation {
    LEFT,
    RIGHT,
    OTHER,
  };

  enum class VerticalLocation {
    TOP,
    BOTTOM,
    OTHER,
  };

  ~MoveLoop() override;

  // If a move/resize loop should occur for the specified parameters creates
  // and returns a new MoveLoop. All events should be funneled to the MoveLoop
  // until done (Move()). |ht_location| is one of the constants defined by
  // HitTestCompat.
  static scoped_ptr<MoveLoop> Create(mus::Window* target,
                                     int ht_location,
                                     const mus::mojom::Event& event);

  // Processes an event for a move/resize loop.
  MoveResult Move(const mus::mojom::Event& event);

  // If possible reverts any changes made during the move loop.
  void Revert();

 private:
  enum class Type {
    MOVE,
    RESIZE,
  };

  MoveLoop(mus::Window* target,
           const mus::mojom::Event& event,
           Type type,
           HorizontalLocation h_loc,
           VerticalLocation v_loc);

  // Determines the type of move from the specified HitTestCompat value.
  // Returns true if a move/resize should occur.
  static bool DetermineType(int ht_location,
                            Type* type,
                            HorizontalLocation* h_loc,
                            VerticalLocation* v_loc);

  // Does the actual move/resize.
  void MoveImpl(const mus::mojom::Event& event);

  // Cancels the loop. This sets |target_| to null and removes the observer.
  // After this the MoveLoop is still ongoing and won't stop until the
  // appropriate event is received.
  void Cancel();

  gfx::Rect DetermineBoundsFromDelta(const gfx::Vector2d& delta);

  // mus::WindowObserver:
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowVisibilityChanged(mus::Window* window) override;

  // The window this MoveLoop is acting on. |target_| is set to null if the
  // window unexpectedly changes while the move is in progress.
  mus::Window* target_;

  const Type type_;
  const HorizontalLocation h_loc_;
  const VerticalLocation v_loc_;

  // The id of the pointer that triggered the move.
  const int32_t pointer_id_;

  // Location of the event (in screen coordinates) that triggered the move.
  const gfx::Point initial_event_screen_location_;

  // Original bounds of the window.
  const gfx::Rect initial_window_bounds_;

  const gfx::Rect initial_user_set_bounds_;

  // Set to true when MoveLoop changes the bounds of |target_|. The move is
  // canceled if the bounds change unexpectedly during the move.
  bool changing_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MoveLoop);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_FRAME_MOVE_LOOP_H_
