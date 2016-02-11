// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_
#define COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "components/mus/public/interfaces/input_event_constants.mojom.h"
#include "components/mus/public/interfaces/input_event_matcher.mojom.h"
#include "components/mus/public/interfaces/input_events.mojom.h"
#include "components/mus/public/interfaces/input_key_codes.mojom.h"
#include "components/mus/ws/server_window_observer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace mus {
namespace ws {

class EventDispatcherDelegate;
class EventMatcher;
class ServerWindow;

// Handles dispatching events to the right location as well as updating focus.
class EventDispatcher : public ServerWindowObserver {
 public:
  explicit EventDispatcher(EventDispatcherDelegate* delegate);
  ~EventDispatcher() override;

  void set_root(ServerWindow* root) { root_ = root; }

  void set_surface_id(cc::SurfaceId surface_id) { surface_id_ = surface_id; }

  // |capture_window_| will receive all input. See window_tree.mojom for
  // details.
  ServerWindow* capture_window() { return capture_window_; }
  const ServerWindow* capture_window() const { return capture_window_; }
  void SetCaptureWindow(ServerWindow* capture_window, bool in_nonclient_area);

  // Retrieves the ServerWindow of the last mouse move.
  ServerWindow* mouse_cursor_source_window() const {
    return mouse_cursor_source_window_;
  }

  // Possibly updates the cursor. If we aren't in an implicit capture, we take
  // the last known location of the mouse pointer, and look for the
  // ServerWindow* under it.
  void UpdateCursorProviderByLastKnownLocation();

  // Adds an accelerator with the given id and event-matcher. If an accelerator
  // already exists with the same id or the same matcher, then the accelerator
  // is not added. Returns whether adding the accelerator was successful or not.
  bool AddAccelerator(uint32_t id, mojom::EventMatcherPtr event_matcher);
  void RemoveAccelerator(uint32_t id);

  // Processes the supplied event, informing the delegate as approriate. This
  // may result in generating any number of events.
  void ProcessEvent(mojom::EventPtr event);

 private:
  friend class EventDispatcherTest;

  // Keeps track of state associated with an active pointer.
  struct PointerTarget {
    PointerTarget()
        : window(nullptr), in_nonclient_area(false), is_pointer_down(false) {}

    // NOTE: this is set to null if the window is destroyed before a
    // corresponding release/cancel.
    ServerWindow* window;

    // Did the pointer event start in the non-client area.
    bool in_nonclient_area;

    bool is_pointer_down;
  };

  void ProcessKeyEvent(mojom::EventPtr event);

  bool IsTrackingPointer(int32_t pointer_id) const {
    return pointer_targets_.count(pointer_id) > 0;
  }

  // EventDispatcher provides the following logic for pointer events:
  // . wheel events go to the current target of the associated pointer. If
  //   there is no target, they go to the deepest window.
  // . move (not drag) events go to the deepest window.
  // . when a pointer goes down all events until the corresponding up or
  //   cancel go to the deepest target. For mouse events the up only occurs
  //   when no buttons on the mouse are down.
  // This also generates exit events as appropriate. For example, if the mouse
  // moves between one window to another an exit is generated on the first.
  void ProcessPointerEvent(mojom::EventPtr event);

  // Adds |pointer_target| to |pointer_targets_|.
  void StartTrackingPointer(int32_t pointer_id,
                            const PointerTarget& pointer_target);

  // Removes a PointerTarget from |pointer_targets_|.
  void StopTrackingPointer(int32_t pointer_id);

  // Starts tracking the pointer for |event|, or if already tracking the
  // pointer sends the appropriate event to the delegate and updates the
  // currently tracked PointerTarget appropriately.
  void UpdateTargetForPointer(const mojom::Event& event);

  // Returns a PointerTarget from the supplied Event.
  PointerTarget PointerTargetForEvent(const mojom::Event& event) const;

  // Returns true if any pointers are in the pressed/down state.
  bool AreAnyPointersDown() const;

  // If |target->window| is valid, then passes the event to the delegate.
  void DispatchToPointerTarget(const PointerTarget& target,
                               mojom::EventPtr event);

  // Stops sending pointer events to |window|. This does not remove the entry
  // for |window| from |pointer_targets_|, rather it nulls out the window. This
  // way we continue to eat events until the up/cancel is received.
  void CancelPointerEventsToTarget(ServerWindow* window);

  // Returns true if we're currently an observer for |window|. We are an
  // observer for a window if any pointer events are targeting it.
  bool IsObservingWindow(ServerWindow* window);

  // Looks to see if there is an accelerator bound to the specified code/flags.
  // If there is one, sets |accelerator_id| to the id of the accelerator invoked
  // and returns true. If there is none, returns false so normal key event
  // processing can continue.
  bool FindAccelerator(const mojom::Event& event, uint32_t* accelerator_id);

  // ServerWindowObserver:
  void OnWillChangeWindowHierarchy(ServerWindow* window,
                                   ServerWindow* new_parent,
                                   ServerWindow* old_parent) override;
  void OnWindowVisibilityChanged(ServerWindow* window) override;
  void OnWindowDestroyed(ServerWindow* window) override;

  EventDispatcherDelegate* delegate_;
  ServerWindow* root_;
  ServerWindow* capture_window_;

  bool capture_window_in_nonclient_area_;
  bool mouse_button_down_;
  ServerWindow* mouse_cursor_source_window_;

  // The on screen location of the mouse pointer. This can be outside the
  // bounds of |mouse_cursor_source_window_|, which can capture the cursor.
  gfx::Point mouse_pointer_last_location_;

  cc::SurfaceId surface_id_;

  using Entry = std::pair<uint32_t, EventMatcher>;
  std::map<uint32_t, EventMatcher> accelerators_;

  using PointerIdToTargetMap = std::map<int32_t, PointerTarget>;
  // |pointer_targets_| contains the active pointers. For a mouse based pointer
  // a PointerTarget is always active (and present in |pointer_targets_|). For
  // touch based pointers the pointer is active while down and removed on
  // cancel or up.
  PointerIdToTargetMap pointer_targets_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_
