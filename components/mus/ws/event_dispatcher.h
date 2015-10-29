// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_
#define COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_

#include <map>

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

  void AddAccelerator(uint32_t id, mojom::EventMatcherPtr event_matcher);
  void RemoveAccelerator(uint32_t id);

  void OnEvent(mojom::EventPtr event);

 private:
  // Keeps track of state associated with a pointer down until the
  // corresponding up/cancel.
  struct PointerTarget {
    PointerTarget() : window(nullptr), in_nonclient_area(false) {}

    // NOTE: this is set to null if the window is destroyed before a
    // corresponding release/cancel.
    ServerWindow* window;

    // Did the pointer event start in the non-client area.
    bool in_nonclient_area;
  };

  void ProcessKeyEvent(mojom::EventPtr event);

  // EventDispatcher provides the following logic for pointer events:
  // . wheel events go to the current target of the associated pointer. If
  //   there is no target, they go to the deepest window.
  // . move (not drag) events go to the deepest window.
  // . when a pointer goes down all events until the corresponding up or
  //   cancel go to the deepest target. For mouse events the up only occurs
  //   when no buttons on the mouse are down.
  void ProcessPointerEvent(mojom::EventPtr event);

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

  cc::SurfaceId surface_id_;

  using Entry = std::pair<uint32_t, EventMatcher>;
  std::map<uint32_t, EventMatcher> accelerators_;

  using PointerIdToTargetMap = std::map<int32_t, PointerTarget>;
  PointerIdToTargetMap pointer_targets_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_
