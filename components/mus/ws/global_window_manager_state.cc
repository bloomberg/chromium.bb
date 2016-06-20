// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/global_window_manager_state.h"

#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/user_display_manager.h"
#include "components/mus/ws/window_tree.h"

namespace mus {
namespace ws {

GlobalWindowManagerState::GlobalWindowManagerState(WindowTree* window_tree)
    : window_tree_(window_tree) {
  frame_decoration_values_ = mojom::FrameDecorationValues::New();
  frame_decoration_values_->max_title_bar_button_width = 0u;
}

GlobalWindowManagerState::~GlobalWindowManagerState() {}

void GlobalWindowManagerState::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  got_frame_decoration_values_ = true;
  frame_decoration_values_ = values.Clone();
  window_tree_->display_manager()
      ->GetUserDisplayManager(window_tree_->user_id())
      ->OnFrameDecorationValuesChanged();
}

}  // namespace ws
}  // namespace mus
