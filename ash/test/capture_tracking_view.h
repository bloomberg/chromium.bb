// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_CAPTURE_TRACKING_VIEW_
#define ASH_TEST_CAPTURE_TRACKING_VIEW_
#pragma once

#include "base/compiler_specific.h"
#include "ui/views/view.h"

namespace ash {
namespace test {

// Used to track OnMousePressed() and OnMouseCaptureLost().
class CaptureTrackingView : public views::View {
 public:
  CaptureTrackingView();
  virtual ~CaptureTrackingView();

  // Returns true if OnMousePressed() has been invoked.
  bool got_press() const { return got_press_; }

  // Returns true if OnMouseCaptureLost() has been invoked.
  bool got_capture_lost() const { return got_capture_lost_; }

  void reset() { got_press_ = got_capture_lost_ = false; }

  // Overridden from views::View
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;

 private:
  // See description above getters.
  bool got_press_;
  bool got_capture_lost_;

  DISALLOW_COPY_AND_ASSIGN(CaptureTrackingView);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_CAPTURE_TRACKING_VIEW_
