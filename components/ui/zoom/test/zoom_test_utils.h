// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_ZOOM_ZOOM_TEST_UTILS_H_
#define COMPONENTS_UI_ZOOM_ZOOM_TEST_UTILS_H_

#include "base/macros.h"
#include "components/ui/zoom/zoom_controller.h"
#include "components/ui/zoom/zoom_observer.h"

namespace content {
class MessageLoopRunner;
}

namespace ui_zoom {

bool operator==(const ZoomController::ZoomChangedEventData& lhs,
                const ZoomController::ZoomChangedEventData& rhs);

class ZoomChangedWatcher : public ui_zoom::ZoomObserver {
 public:
  ZoomChangedWatcher(
      ZoomController* zoom_controller,
      const ZoomController::ZoomChangedEventData& expected_event_data);
  ~ZoomChangedWatcher() override;

  void Wait();

  void OnZoomChanged(
      const ZoomController::ZoomChangedEventData& event_data) override;

 private:
  ZoomController* zoom_controller_;
  ZoomController::ZoomChangedEventData expected_event_data_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ZoomChangedWatcher);
};

}  // namespace ui_zoom
#endif  // COMPONENTS_UI_ZOOM_ZOOM_TEST_UTILS_H_
