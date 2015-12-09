// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/default_ash_event_generator_delegate.h"

#include "chrome/browser/ui/host_desktop.h"

DefaultAshEventGeneratorDelegate*
DefaultAshEventGeneratorDelegate::GetInstance() {
  return base::Singleton<DefaultAshEventGeneratorDelegate>::get();
}

void DefaultAshEventGeneratorDelegate::SetContext(
    ui::test::EventGenerator* owner,
    gfx::NativeWindow root_window,
    gfx::NativeWindow window) {
  root_window_ = root_window;
}

aura::WindowTreeHost* DefaultAshEventGeneratorDelegate::GetHostAt(
    const gfx::Point& point) const {
  return root_window_->GetHost();
}

aura::client::ScreenPositionClient*
DefaultAshEventGeneratorDelegate::GetScreenPositionClient(
    const aura::Window* window) const {
  return nullptr;
}

void DefaultAshEventGeneratorDelegate::DispatchKeyEventToIME(
    ui::EventTarget* target,
    ui::KeyEvent* event) {
  // In Ash environment, the key event will be processed by event rewriters
  // first and event will not be hanlded.
  // Otherwise, use EventGeneratorDelegateAura::DispatchKeyEventToIME() to
  // dispatch |event| to the input method.
  if (chrome::GetActiveDesktop() != chrome::HOST_DESKTOP_TYPE_ASH) {
    EventGeneratorDelegateAura::DispatchKeyEventToIME(target, event);
  }
}

DefaultAshEventGeneratorDelegate::DefaultAshEventGeneratorDelegate()
    : root_window_(nullptr) {
  DCHECK(!ui::test::EventGenerator::default_delegate);
  ui::test::EventGenerator::default_delegate = this;
}

DefaultAshEventGeneratorDelegate::~DefaultAshEventGeneratorDelegate() {
  DCHECK_EQ(this, ui::test::EventGenerator::default_delegate);
  ui::test::EventGenerator::default_delegate = nullptr;
}
