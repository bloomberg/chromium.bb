// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui/zoom/test/zoom_test_utils.h"

#include "content/public/test/test_utils.h"

namespace ui_zoom {

bool operator==(const ZoomController::ZoomChangedEventData& lhs,
                const ZoomController::ZoomChangedEventData& rhs) {
  return lhs.web_contents == rhs.web_contents &&
         lhs.old_zoom_level == rhs.old_zoom_level &&
         lhs.new_zoom_level == rhs.new_zoom_level &&
         lhs.zoom_mode == rhs.zoom_mode &&
         lhs.can_show_bubble == rhs.can_show_bubble;
}

ZoomChangedWatcher::ZoomChangedWatcher(
    ZoomController* zoom_controller,
    const ZoomController::ZoomChangedEventData& expected_event_data)
    : zoom_controller_(zoom_controller),
      expected_event_data_(expected_event_data),
      message_loop_runner_(new content::MessageLoopRunner) {
    zoom_controller_->AddObserver(this);
  }

ZoomChangedWatcher::~ZoomChangedWatcher() {
  zoom_controller_->RemoveObserver(this);
}

void ZoomChangedWatcher::Wait() {
  message_loop_runner_->Run();
}

void ZoomChangedWatcher::OnZoomChanged(
    const ZoomController::ZoomChangedEventData& event_data) {
  if (event_data == expected_event_data_)
    message_loop_runner_->Quit();
}

}  // namespace ui_zoom
