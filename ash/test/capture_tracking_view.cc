// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/capture_tracking_view.h"

namespace ash {
namespace test {

CaptureTrackingView::CaptureTrackingView()
    : got_press_(false),
      got_capture_lost_(false) {
}

CaptureTrackingView::~CaptureTrackingView() {
}

bool CaptureTrackingView::OnMousePressed(const views::MouseEvent& event) {
  got_press_ = true;
  return true;
}

void CaptureTrackingView::OnMouseCaptureLost() {
  got_capture_lost_ = true;
}

}  // namespace test
}  // namespace ash
