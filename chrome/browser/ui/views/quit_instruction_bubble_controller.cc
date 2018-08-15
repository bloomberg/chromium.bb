// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/quit_instruction_bubble_controller.h"

#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/quit_instruction_bubble.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

constexpr ui::KeyboardCode kAcceleratorKeyCode = ui::VKEY_Q;
constexpr int kAcceleratorModifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN;

constexpr base::TimeDelta kShowDuration =
    base::TimeDelta::FromMilliseconds(3000);

}  // namespace

// static
QuitInstructionBubbleController*
QuitInstructionBubbleController::GetInstance() {
  return base::Singleton<QuitInstructionBubbleController>::get();
}

QuitInstructionBubbleController::QuitInstructionBubbleController()
    : QuitInstructionBubbleController(std::make_unique<QuitInstructionBubble>(),
                                      std::make_unique<base::OneShotTimer>()) {}

QuitInstructionBubbleController::QuitInstructionBubbleController(
    std::unique_ptr<QuitInstructionBubbleBase> bubble,
    std::unique_ptr<base::OneShotTimer> hide_timer)
    : view_(std::move(bubble)), hide_timer_(std::move(hide_timer)) {}

QuitInstructionBubbleController::~QuitInstructionBubbleController() {}

void QuitInstructionBubbleController::OnKeyEvent(ui::KeyEvent* event) {
  const ui::Accelerator accelerator(*event);
  if (accelerator.key_code() == kAcceleratorKeyCode &&
      accelerator.modifiers() == kAcceleratorModifiers) {
    event->SetHandled();
    if (accelerator.key_state() == ui::Accelerator::KeyState::PRESSED) {
      view_->Show();
      hide_timer_->Start(FROM_HERE, kShowDuration, this,
                         &QuitInstructionBubbleController::OnTimerElapsed);
    }
  }
}

void QuitInstructionBubbleController::OnTimerElapsed() {
  view_->Hide();
}
