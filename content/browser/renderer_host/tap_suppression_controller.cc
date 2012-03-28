// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/tap_suppression_controller.h"

#include <utility>

// The default implementation of the TapSuppressionController does not
// suppress taps. Tap suppression is needed only on CrOS.

namespace content {

TapSuppressionController::TapSuppressionController(RenderWidgetHostImpl*)
    : render_widget_host_(NULL) {}

TapSuppressionController::~TapSuppressionController() {}

bool TapSuppressionController::ShouldSuppressMouseUp() { return false; }

bool TapSuppressionController::ShouldDeferMouseDown(
    const WebKit::WebMouseEvent& event) {
  return false;
}

void TapSuppressionController::GestureFlingCancelAck(bool) {}
void TapSuppressionController::GestureFlingCancel(double) {}
void TapSuppressionController::MouseDownTimerExpired() {}

} // namespace content.
