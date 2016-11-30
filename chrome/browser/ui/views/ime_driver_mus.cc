// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver_mus.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
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

IMEDriver::~IMEDriver() {}

// static
void IMEDriver::Register() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ui::mojom::IMEDriverPtr ime_driver_ptr;
  mojo::MakeStrongBinding(base::WrapUnique(new IMEDriver),
                          GetProxy(&ime_driver_ptr));
  ui::mojom::IMERegistrarPtr ime_registrar;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->ConnectToInterface(ui::mojom::kServiceName, &ime_registrar);
  ime_registrar->RegisterDriver(std::move(ime_driver_ptr));
}

void IMEDriver::StartSession(
    int32_t session_id,
    ui::mojom::TextInputClientPtr client,
    ui::mojom::InputMethodRequest input_method_request) {
  input_method_bindings_[session_id] =
      base::MakeUnique<mojo::Binding<ui::mojom::InputMethod>>(
          new InputMethod(std::move(client)), std::move(input_method_request));
}

IMEDriver::IMEDriver() {}

void IMEDriver::CancelSession(int32_t session_id) {
  input_method_bindings_.erase(session_id);
}
