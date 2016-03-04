// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"

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
  WindowManagerState(Display* display, uint32_t user_id);
  ~WindowManagerState();

  bool is_user_id_valid() const { return is_user_id_valid_; }

  ServerWindow* root() { return root_.get(); }
  const ServerWindow* root() const { return root_.get(); }

  WindowTree* tree() { return tree_; }
  const WindowTree* tree() const { return tree_; }

 private:
  friend class Display;

  WindowManagerState(Display* display, bool is_user_id_valid, uint32_t user_id);

  Display* display_;
  // If this was created implicitly by a call
  // WindowTreeHostFactory::CreateWindowTreeHost(), then |is_user_id_valid_|
  // is false.
  const bool is_user_id_valid_;
  const uint32_t user_id_;
  scoped_ptr<ServerWindow> root_;
  WindowTree* tree_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerState);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_
