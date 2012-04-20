// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PARTIAL_SCREENSHOT_EVENT_FILTER_H_
#define ASH_WM_PARTIAL_SCREENSHOT_EVENT_FILTER_H_
#pragma once

#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"

namespace ash {
class PartialScreenshotView;

namespace internal {

// EventFilter for the partial screenshot UI.  It does nothing
// for the first time, but works when |Activate()| is called.  The
// main task of this event filter is just to stop propagation of any
// key events during activation, and also signal cancellation when Esc is
// pressed.
class PartialScreenshotEventFilter : public aura::EventFilter,
                                     public ShellObserver {
 public:
  PartialScreenshotEventFilter();
  virtual ~PartialScreenshotEventFilter();

  // Start the filtering of events.  It also notifies the specified
  // |view| when a key event means cancel (like Esc).  It holds the
  // pointer to the specified |view| until Deactivate() is called, but
  // does not take ownership.
  void Activate(PartialScreenshotView* view);

  // End the filtering of events.
  void Deactivate();

  // Cancels the partial screenshot UI.  Do nothing if it's not activated.
  void Cancel();

  // aura::EventFilter overrides:
  virtual bool PreHandleKeyEvent(
      aura::Window* target, aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(
      aura::Window* target, aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target, aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target, aura::GestureEvent* event) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnLoginStateChanged(user::LoginStatus status) OVERRIDE;
  virtual void OnAppTerminating() OVERRIDE;
  virtual void OnLockStateChanged(bool locked) OVERRIDE;

 private:
  PartialScreenshotView* view_;

  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_PARTIAL_SCREENSHOT_EVENT_FILTER_H_
