// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ime_driver_ash.h"

#include "services/ui/public/interfaces/ime.mojom.h"

namespace {

class InputMethod : public ui::mojom::InputMethod {
 public:
  explicit InputMethod(ui::mojom::TextInputClientPtr client)
      : client_(std::move(client)) {}
  ~InputMethod() override {}

 private:
  // ui::mojom::InputMethod:
  void OnTextInputModeChanged(
      ui::mojom::TextInputMode text_input_mode) override {}
  void OnTextInputTypeChanged(
      ui::mojom::TextInputType text_input_type) override {}
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {}
  void ProcessKeyEvent(std::unique_ptr<ui::Event> key_event,
                       const ProcessKeyEventCallback& callback) override {
    DCHECK(key_event->IsKeyEvent());

    if (key_event->AsKeyEvent()->is_char()) {
      ui::mojom::CompositionEventPtr composition_event =
          ui::mojom::CompositionEvent::New();
      composition_event->type = ui::mojom::CompositionEventType::INSERT_CHAR;
      composition_event->key_event = std::move(key_event);
      client_->OnCompositionEvent(std::move(composition_event));
      callback.Run(true);
    } else {
      callback.Run(false);
    }
  }
  void CancelComposition() override {}

  ui::mojom::TextInputClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(InputMethod);
};

}  // namespace

IMEDriver::IMEDriver() {}

IMEDriver::~IMEDriver() {}

void IMEDriver::StartSession(
    int32_t session_id,
    ui::mojom::TextInputClientPtr client,
    ui::mojom::InputMethodRequest input_method_request) {
  input_method_bindings_[session_id] =
      base::MakeUnique<mojo::Binding<ui::mojom::InputMethod>>(
          new InputMethod(std::move(client)), std::move(input_method_request));
}

void IMEDriver::CancelSession(int32_t session_id) {
  input_method_bindings_.erase(session_id);
}
