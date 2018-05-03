// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFIRM_QUIT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONFIRM_QUIT_BUBBLE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"

class ConfirmQuitBubbleBase;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {
class Accelerator;
}

// Manages showing and hiding the confirm-to-quit bubble.  Requests Chrome to be
// closed if the quit accelerator is held down or pressed twice in succession.
class ConfirmQuitBubbleController {
 public:
  static ConfirmQuitBubbleController* GetInstance();

  ~ConfirmQuitBubbleController();

  // Returns true if the event was handled.
  bool HandleKeyboardEvent(const ui::Accelerator& accelerator);

 private:
  friend struct base::DefaultSingletonTraits<ConfirmQuitBubbleController>;
  friend class ConfirmQuitBubbleControllerTest;

  ConfirmQuitBubbleController(std::unique_ptr<ConfirmQuitBubbleBase> bubble,
                              std::unique_ptr<base::Timer> hide_timer);

  ConfirmQuitBubbleController();

  void OnTimerElapsed();

  void Quit();

  void SetQuitActionForTest(base::OnceClosure quit_action);

  std::unique_ptr<ConfirmQuitBubbleBase> const view_;

  // Indicates if the accelerator was released while the timer was active.
  bool released_key_ = false;

  std::unique_ptr<base::Timer> hide_timer_;

  base::OnceClosure quit_action_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmQuitBubbleController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFIRM_QUIT_BUBBLE_CONTROLLER_H_
