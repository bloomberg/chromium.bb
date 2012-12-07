// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_activation_delegate.h"

#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/views/corewm/focus_change_event.h"

namespace ash {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// TestActivationDelegate

TestActivationDelegate::TestActivationDelegate()
    : window_(NULL),
      window_was_active_(false),
      activate_(true),
      activated_count_(0),
      lost_active_count_(0),
      should_activate_count_(0) {
}

TestActivationDelegate::TestActivationDelegate(bool activate)
    : window_(NULL),
      window_was_active_(false),
      activate_(activate),
      activated_count_(0),
      lost_active_count_(0),
      should_activate_count_(0) {
}

void TestActivationDelegate::SetWindow(aura::Window* window) {
  window_ = window;
  aura::client::SetActivationDelegate(window, this);
  window_->AddPreTargetHandler(this);
}

bool TestActivationDelegate::ShouldActivate() const {
  should_activate_count_++;
  return activate_;
}

void TestActivationDelegate::OnActivated() {
  activated_count_++;
}

void TestActivationDelegate::OnLostActive() {
  if (lost_active_count_++ == 0)
    window_was_active_ = wm::IsActiveWindow(window_);
}

void TestActivationDelegate::OnEvent(ui::Event* event) {
  if (event->target() == window_) {
    if (event->type() ==
        views::corewm::FocusChangeEvent::activation_changed_event_type()) {
      OnActivated();
    } else if (event->type() ==
        views::corewm::FocusChangeEvent::activation_changing_event_type()) {
      OnLostActive();
    }
  }
  EventHandler::OnEvent(event);
}

}  // namespace test
}  // namespace ash
