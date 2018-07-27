// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_CHROMEOS_H_

#include "chrome/browser/ui/views/ime_driver/remote_text_input_client.h"
#include "services/ui/public/interfaces/ime/ime.mojom.h"

class AccessibilityInputMethodObserver;

namespace ui {
class InputMethodChromeOS;
}

// This bridges between mojo InputMethod API and ui::InputMethodChromeOS. It
// forwards the received events to an instance of ui::InputMethodChromeOS.
// Under mash this object is created and destroyed as top-level windows gain
// focus and start IME sessions.
// NOTE: There may be multiple instances of InputMethodChromeOS. In classic ash
// there is one instance shared by ash and browser, plus one per remote app
// (e.g. shortcut viewer).
class InputMethodBridge : public ui::mojom::InputMethod {
 public:
  explicit InputMethodBridge(std::unique_ptr<RemoteTextInputClient> client);
  ~InputMethodBridge() override;

  // ui::mojom::InputMethod:
  void OnTextInputTypeChanged(ui::TextInputType text_input_type) override;
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override;
  void ProcessKeyEvent(std::unique_ptr<ui::Event> key_event,
                       ProcessKeyEventCallback callback) override;
  void CancelComposition() override;

 private:
  std::unique_ptr<RemoteTextInputClient> client_;
  std::unique_ptr<ui::InputMethodChromeOS> input_method_chromeos_;
  // Must be destroyed before |input_method_chromeos_|.
  std::unique_ptr<AccessibilityInputMethodObserver>
      accessibility_input_method_observer_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBridge);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_CHROMEOS_H_
