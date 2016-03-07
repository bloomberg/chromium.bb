// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/ws/user_id.h"

namespace mus {
namespace ws {

class Display;
class ServerWindow;
class WindowTree;

// Manages the state associated with a connection to a WindowManager for
// a specific user.
class WindowManagerState {
 public:
  // Creates a WindowManagerState that can host content from any user.
  explicit WindowManagerState(Display* display);
  WindowManagerState(Display* display, const UserId& user_id);
  ~WindowManagerState();

  bool is_user_id_valid() const { return is_user_id_valid_; }

  ServerWindow* root() { return root_.get(); }
  const ServerWindow* root() const { return root_.get(); }

  WindowTree* tree() { return tree_; }
  const WindowTree* tree() const { return tree_; }

  Display* display() { return display_; }
  const Display* display() const { return display_; }

  void SetFrameDecorationValues(mojom::FrameDecorationValuesPtr values);
  const mojom::FrameDecorationValues& frame_decoration_values() const {
    return *frame_decoration_values_;
  }
  bool got_frame_decoration_values() const {
    return got_frame_decoration_values_;
  }

  // Returns a mojom::Display for the specified display. WindowManager specific
  // values are not set.
  mojom::DisplayPtr ToMojomDisplay() const;

 private:
  friend class Display;

  WindowManagerState(Display* display, bool is_user_id_valid,
                     const UserId& user_id);

  Display* display_;
  // If this was created implicitly by a call
  // WindowTreeHostFactory::CreateWindowTreeHost(), then |is_user_id_valid_|
  // is false.
  const bool is_user_id_valid_;
  const UserId user_id_;
  scoped_ptr<ServerWindow> root_;
  WindowTree* tree_ = nullptr;

  // Set to true the first time SetFrameDecorationValues() is received.
  bool got_frame_decoration_values_ = false;
  mojom::FrameDecorationValuesPtr frame_decoration_values_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerState);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_
