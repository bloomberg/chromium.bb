// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_WINDOW_TRACKER_H_
#define COMPONENTS_MUS_WS_SERVER_WINDOW_TRACKER_H_

#include <stdint.h>
#include <set>

#include "base/macros.h"
#include "components/mus/common/window_tracker.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_observer.h"

namespace mus {
namespace ws {

using ServerWindowTracker =
    WindowTrackerTemplate<ServerWindow, ServerWindowObserver>;

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_WINDOW_TRACKER_H_
