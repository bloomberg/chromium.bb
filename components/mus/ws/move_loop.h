// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_MOVE_LOOP_H_
#define COMPONENTS_MUS_WS_MOVE_LOOP_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/ws/server_window_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace mus {
namespace ws {

class ServerWindow;

// MoveLoop is responsible for moving/resizing windows. EventDispatcher
// attempts to create a MoveLoop on every POINTER_DOWN event. Once a MoveLoop
// is created it is fed events until an event is received that stops the loop.
class MoveLoop : public ServerWindowObserver {
 public:
  enum MoveResult {
    // The move is still ongoing.
    CONTINUE,

    // The move is done and the MoveLoop should be destroyed.
    DONE,
  };

  ~MoveLoop() override;

  // If a move/resize loop should occur for the specified parameters creates
  // and returns a new MoveLoop. All events should be funneled to the MoveLoop
  // until done (Move()).
  static scoped_ptr<MoveLoop> Create(ServerWindow* target,
                                     const mojo::Event& event);

  // Processes an event for a move/resize loop.
  MoveResult Move(const mojo::Event& event);

 private:
  MoveLoop(ServerWindow* target, const mojo::Event& event);

  // Does the actual move/resize.
  void MoveImpl(const mojo::Event& event);

  // Cancels the loop. This sets |target_| to null and removes the observer.
  // After this the MoveLoop is still ongoing and won't stop until the
  // appropriate event is received.
  void Cancel();

  void Revert();

  // ServerWindowObserver:
  void OnWindowHierarchyChanged(ServerWindow* window,
                                ServerWindow* new_parent,
                                ServerWindow* old_parent) override;
  void OnWindowBoundsChanged(ServerWindow* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowVisibilityChanged(ServerWindow* window) override;

  // The window this MoveLoop is acting on. |target_| is set to null if the
  // window unexpectedly changes while the move is in progress.
  ServerWindow* target_;

  // The id of the pointer that triggered the move.
  const int32_t pointer_id_;

  // Location of the event (in screen coordinates) that triggered the move.
  const gfx::Point initial_event_screen_location_;

  // Original bounds of the window.
  const gfx::Rect initial_window_bounds_;

  // Set to true when MoveLoop changes the bounds of |target_|. The move is
  // canceled if the bounds change unexpectedly during the move.
  bool changing_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MoveLoop);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_MOVE_LOOP_H_
