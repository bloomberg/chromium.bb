// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MAGNIFIER_MAGNIFIER_KEY_SCROLLER_H_
#define ASH_MAGNIFIER_MAGNIFIER_KEY_SCROLLER_H_

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"

namespace ui {
class KeyEvent;
}

namespace ash {

// This class implements the press and hold key-bindings (shift-arrow keys)
// to control magnified screen.
class ASH_EXPORT MagnifierKeyScroller : public ui::EventHandler {
 public:
  static bool IsEnabled();
  static void SetEnabled(bool enabled);

  MagnifierKeyScroller();
  virtual ~MagnifierKeyScroller();

  // ui::EventHandler overrides:
  virtual void OnKeyEvent(ui::KeyEvent* key_event) OVERRIDE;

  // A scoped object to enable and disable the magnifier accelerator for test.
  class ScopedEnablerForTest {
   public:
    ScopedEnablerForTest() {
      SetEnabled(true);
    }
    ~ScopedEnablerForTest() {
      SetEnabled(false);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedEnablerForTest);
  };

 private:
  // A state to keep track of one click and click and hold operation.
  //
  // One click:
  // INITIAL --(first press)--> PRESSED --(release)--> INITIAL[SEND PRESS]
  //
  // Click and hold:
  // INITIAL --(first press)--> PRESSED --(press)-->
  //   HOLD[scroll] --(press)--> HOLD[scroll] --(release)-->
  //   INITIAL[stop scroll]
  enum State {
    INITIAL,
    PRESSED,
    HOLD
  };

  State state_;

  DISALLOW_COPY_AND_ASSIGN(MagnifierKeyScroller);
};

}  // namespace ash

#endif  // ASH_MAGNIFIER_MAGNIFIER_KEY_SCROLLER_H_
