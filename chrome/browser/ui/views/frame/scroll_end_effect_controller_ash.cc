// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/scroll_end_effect_controller_ash.h"

ScrollEndEffectController* ScrollEndEffectController::Create() {
  return new ScrollEndEffectControllerAsh();
}

ScrollEndEffectControllerAsh::ScrollEndEffectControllerAsh() {
}

ScrollEndEffectControllerAsh::~ScrollEndEffectControllerAsh() {
}

void ScrollEndEffectControllerAsh::OverscrollUpdate(int delta_y) {
  // TODO(rharrison): Implement initial version of scroll end effect
}
