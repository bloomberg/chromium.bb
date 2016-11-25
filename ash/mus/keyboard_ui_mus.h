// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_KEYBOARD_UI_MUS_H_
#define ASH_MUS_KEYBOARD_UI_MUS_H_

#include <stdint.h>

#include <memory>

#include "ash/common/keyboard/keyboard_ui.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/keyboard/keyboard.mojom.h"

namespace service_manager {
class Connector;
}

namespace ash {

class KeyboardUIMus : public KeyboardUI,
                      public keyboard::mojom::KeyboardObserver {
 public:
  explicit KeyboardUIMus(service_manager::Connector* connector);
  ~KeyboardUIMus() override;

  static std::unique_ptr<KeyboardUI> Create(
      service_manager::Connector* connector);

  // KeyboardUI:
  void Hide() override;
  void Show() override;
  void ShowInDisplay(const int64_t display_id) override;
  bool IsEnabled() override;

  // keyboard::mojom::KeyboardObserver:
  void OnKeyboardStateChanged(bool is_enabled,
                              bool is_visible,
                              uint64_t display_id,
                              const gfx::Rect& bounds) override;

 private:
  bool is_enabled_;

  keyboard::mojom::KeyboardPtr keyboard_;
  mojo::Binding<keyboard::mojom::KeyboardObserver> observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardUIMus);
};

}  // namespace ash

#endif  // ASH_MUS_KEYBOARD_UI_MUS_H_
