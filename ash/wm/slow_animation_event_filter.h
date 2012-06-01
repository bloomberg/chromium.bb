// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SLOW_ANIMATION_EVENT_FILTER_H_
#define ASH_WM_SLOW_ANIMATION_EVENT_FILTER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/aura/event_filter.h"

namespace ash {
namespace internal {

// An event filter to detect modifier key presses and trigger a special "slow
// animation" mode for visual debugging of layer animations.
// Exported for unit tests.
class ASH_EXPORT SlowAnimationEventFilter : public aura::EventFilter {
 public:
  SlowAnimationEventFilter();
  virtual ~SlowAnimationEventFilter();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(SlowAnimationEventFilterTest, Basics);

  // Returns true if this is a raw shift-key-pressed event.
  bool IsUnmodifiedShiftPressed(aura::KeyEvent* event) const;

  DISALLOW_COPY_AND_ASSIGN(SlowAnimationEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SLOW_ANIMATION_EVENT_FILTER_H_
