// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_quit_bubble_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/confirm_quit_bubble.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

constexpr ui::KeyboardCode kAcceleratorKeyCode = ui::VKEY_Q;
constexpr int kAcceleratorModifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN;

constexpr base::TimeDelta kShowDuration =
    base::TimeDelta::FromMilliseconds(1500);

}  // namespace

// static
ConfirmQuitBubbleController* ConfirmQuitBubbleController::GetInstance() {
  return base::Singleton<ConfirmQuitBubbleController>::get();
}

ConfirmQuitBubbleController::ConfirmQuitBubbleController()
    : view_(std::make_unique<ConfirmQuitBubble>()),
      hide_timer_(std::make_unique<base::OneShotTimer>()) {}

ConfirmQuitBubbleController::ConfirmQuitBubbleController(
    std::unique_ptr<ConfirmQuitBubbleBase> bubble,
    std::unique_ptr<base::Timer> hide_timer)
    : view_(std::move(bubble)), hide_timer_(std::move(hide_timer)) {}

ConfirmQuitBubbleController::~ConfirmQuitBubbleController() {}

bool ConfirmQuitBubbleController::HandleKeyboardEvent(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == kAcceleratorKeyCode &&
      accelerator.modifiers() == kAcceleratorModifiers &&
      accelerator.key_state() == ui::Accelerator::KeyState::PRESSED &&
      !accelerator.IsRepeat()) {
    if (!hide_timer_->IsRunning()) {
      view_->Show();
      released_key_ = false;
      hide_timer_->Start(FROM_HERE, kShowDuration, this,
                         &ConfirmQuitBubbleController::OnTimerElapsed);
    } else {
      // The accelerator was pressed while the bubble was showing.  Consider
      // this a confirmation to quit.
      view_->Hide();
      hide_timer_->Stop();
      Quit();
    }
    return true;
  }
  if (accelerator.key_code() == kAcceleratorKeyCode &&
      accelerator.key_state() == ui::Accelerator::KeyState::RELEASED) {
    released_key_ = true;
    return true;
  }
  return false;
}

void ConfirmQuitBubbleController::OnTimerElapsed() {
  view_->Hide();

  if (!released_key_) {
    // The accelerator was held down the entire time the bubble was showing.
    Quit();
  }
}

void ConfirmQuitBubbleController::Quit() {
  if (quit_action_) {
    std::move(quit_action_).Run();
  } else {
    // Delay quitting because doing so destroys objects that may be used when
    // unwinding the stack.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::BindOnce(chrome::Exit));
  }
}

void ConfirmQuitBubbleController::SetQuitActionForTest(
    base::OnceClosure quit_action) {
  quit_action_ = std::move(quit_action);
}
