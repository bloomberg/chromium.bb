// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_event.h"

namespace ash {
namespace wm {

WMEvent::WMEvent(WMEventType type)
    : type_(type) {
}

WMEvent::~WMEvent() {
}

SetBoundsEvent::SetBoundsEvent(WMEventType type, const gfx::Rect& bounds)
    : WMEvent(type),
      requested_bounds_(bounds) {
}

SetBoundsEvent::~SetBoundsEvent() {
}

}  // namespace wm
}  // namespace ash
