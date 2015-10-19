// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_
#define COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_

#include <map>

#include "base/basictypes.h"
#include "ui/mojo/events/input_event_constants.mojom.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "ui/mojo/events/input_key_codes.mojom.h"

namespace gfx {
class Point;
}

namespace mus {

namespace ws {

class ServerWindow;
class WindowTreeHostImpl;

// Handles dispatching events to the right location as well as updating focus.
class EventDispatcher {
 public:
  explicit EventDispatcher(WindowTreeHostImpl* window_tree_host);
  ~EventDispatcher();

  void AddAccelerator(uint32_t id,
                      mojo::KeyboardCode keyboard_code,
                      mojo::EventFlags flags);
  void RemoveAccelerator(uint32_t id);

  void OnEvent(mojo::EventPtr event);

 private:
  struct Accelerator {
    Accelerator(mojo::KeyboardCode keyboard_code, mojo::EventFlags flags)
        : keyboard_code(keyboard_code), flags(flags) {}

    // So we can use this in a set.
    bool operator<(const Accelerator& other) const {
      if (keyboard_code == other.keyboard_code)
        return flags < other.flags;
      return keyboard_code < other.keyboard_code;
    }

    mojo::KeyboardCode keyboard_code;
    mojo::EventFlags flags;
  };

  // Looks to see if there is an accelerator bound to the specified code/flags.
  // If there is one, sets |accelerator_id| to the id of the accelerator invoked
  // and returns true. If there is none, returns false so normal key event
  // processing can continue.
  bool FindAccelerator(const mojo::Event& event, uint32_t* accelerator_id);

  // Returns the ServerWindow that should receive |event|. If |event| is a
  // pointer-type event, then this function also updates the event location to
  // make sure it is in the returned target's coordinate space.
  ServerWindow* FindEventTarget(mojo::Event* event);

  // Finds the deepest visible window that contains the specified location, and
  // updates |location| to be in the returned window's coordinate space.
  ServerWindow* FindDeepestVisibleWindow(ServerWindow* window,
                                         gfx::Point* location);

  // Finds the deepest visible window for the specified location based on
  // surface
  // hit-testing. Updates |location| to be in the returned window's coordinate
  // space.
  ServerWindow* FindDeepestVisibleWindowFromSurface(gfx::Point* location);

  WindowTreeHostImpl* window_tree_host_;

  using Entry = std::pair<uint32_t, Accelerator>;
  std::map<uint32_t, Accelerator> accelerators_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_
