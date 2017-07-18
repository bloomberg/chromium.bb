// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_OVERLAY_DELEGATE_H_
#define ASH_TEST_TEST_OVERLAY_DELEGATE_H_

#include "ash/wm/overlay_event_filter.h"
#include "base/macros.h"

namespace ash {
namespace test {

class TestOverlayDelegate : public OverlayEventFilter::Delegate {
 public:
  TestOverlayDelegate();
  virtual ~TestOverlayDelegate();

  int GetCancelCountAndReset();

 private:
  // Overridden from OverlayEventFilter::Delegate:
  void Cancel() override;
  bool IsCancelingKeyEvent(ui::KeyEvent* event) override;
  aura::Window* GetWindow() override;

  int cancel_count_;

  DISALLOW_COPY_AND_ASSIGN(TestOverlayDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_OVERLAY_DELEGATE_H_
