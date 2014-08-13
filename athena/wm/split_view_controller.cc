// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/split_view_controller.h"

#include "ui/aura/window.h"
#include "ui/events/event_handler.h"

namespace athena {

SplitViewController::SplitViewController() {
}

SplitViewController::~SplitViewController() {
}

void SplitViewController::ScrollBegin(BezelController::Bezel bezel,
                                      float delta) {
}

void SplitViewController::ScrollEnd() {
}

void SplitViewController::ScrollUpdate(float delta) {
}

bool SplitViewController::CanScroll() {
  return false;
}

}  // namespace athena
