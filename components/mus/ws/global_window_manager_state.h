// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_GLOBAL_WINDOW_MANAGER_STATE_H_
#define COMPONENTS_MUS_WS_GLOBAL_WINDOW_MANAGER_STATE_H_

#include <stdint.h>

#include "components/mus/public/interfaces/display.mojom.h"

namespace mus {
namespace ws {

class WindowTree;

// Manages state specific to a WindowManager that is shared across displays.
// GlobalWindowManagerState is owned by the WindowTree the window manager is
// associated with.
class GlobalWindowManagerState {
 public:
  explicit GlobalWindowManagerState(WindowTree* window_tree);
  ~GlobalWindowManagerState();

  void SetFrameDecorationValues(mojom::FrameDecorationValuesPtr values);
  const mojom::FrameDecorationValues& frame_decoration_values() const {
    return *frame_decoration_values_;
  }
  bool got_frame_decoration_values() const {
    return got_frame_decoration_values_;
  }

 private:
  WindowTree* window_tree_;

  // Set to true the first time SetFrameDecorationValues() is called.
  bool got_frame_decoration_values_ = false;
  mojom::FrameDecorationValuesPtr frame_decoration_values_;

  DISALLOW_COPY_AND_ASSIGN(GlobalWindowManagerState);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_GLOBAL_WINDOW_MANAGER_STATE_H_
