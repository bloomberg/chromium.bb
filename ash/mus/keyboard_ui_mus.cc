// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/keyboard_ui_mus.h"

#include "ash/common/keyboard/keyboard_ui_observer.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

KeyboardUIMus::KeyboardUIMus(service_manager::Connector* connector)
    : is_enabled_(false), observer_binding_(this) {
  if (connector) {
    // TODO(sky): should be something like mojo:keyboard, but need mapping.
    connector->ConnectToInterface(content::mojom::kBrowserServiceName,
                                  &keyboard_);
    keyboard_->AddObserver(observer_binding_.CreateInterfacePtrAndBind());
  }
}

KeyboardUIMus::~KeyboardUIMus() {}

// static
std::unique_ptr<KeyboardUI> KeyboardUIMus::Create(
    service_manager::Connector* connector) {
  return base::MakeUnique<KeyboardUIMus>(connector);
}

void KeyboardUIMus::Hide() {
  keyboard_->Hide();
}

void KeyboardUIMus::Show() {
  keyboard_->Show();
}

void KeyboardUIMus::ShowInDisplay(const int64_t display_id) {
  // TODO(yhanada): Send display id after adding a display_id argument to
  // |Keyboard::Show()| in keyboard.mojom. See crbug.com/585253.
  keyboard_->Show();
}

bool KeyboardUIMus::IsEnabled() {
  return is_enabled_;
}

void KeyboardUIMus::OnKeyboardStateChanged(bool is_enabled,
                                           bool is_visible,
                                           uint64_t display_id,
                                           const gfx::Rect& bounds) {
  if (is_enabled_ == is_enabled)
    return;

  is_enabled_ = is_enabled;
  for (auto& observer : *observers())
    observer.OnKeyboardEnabledStateChanged(is_enabled);
}

}  // namespace ash
