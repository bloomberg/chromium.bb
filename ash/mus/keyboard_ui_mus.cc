// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/keyboard_ui_mus.h"

#include "ash/keyboard/keyboard_ui_observer.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "mojo/shell/public/cpp/shell.h"

namespace ash {

KeyboardUIMus::KeyboardUIMus(mojo::Shell* mojo_shell)
    : is_enabled_(false), observer_binding_(this) {
  // TODO(sky): should be something like mojo://keyboard, but need mapping.
  mojo_shell->ConnectToInterface("exe://chrome", &keyboard_);
  keyboard_->AddObserver(observer_binding_.CreateInterfacePtrAndBind());
}

KeyboardUIMus::~KeyboardUIMus() {}

// static
scoped_ptr<KeyboardUI> KeyboardUIMus::Create(mojo::Shell* mojo_shell) {
  return make_scoped_ptr(new KeyboardUIMus(mojo_shell));
}

void KeyboardUIMus::Hide() {
  keyboard_->Hide();
}

void KeyboardUIMus::Show() {
  keyboard_->Show();
}

bool KeyboardUIMus::IsEnabled() {
  return is_enabled_;
}

void KeyboardUIMus::OnKeyboardStateChanged(bool is_enabled,
                                           bool is_visible,
                                           uint64_t display_id,
                                           mojo::RectPtr bounds) {
  if (is_enabled_ == is_enabled)
    return;

  is_enabled_ = is_enabled;
  FOR_EACH_OBSERVER(KeyboardUIObserver, *observers(),
                    OnKeyboardEnabledStateChanged(is_enabled));
}

}  // namespace ash
