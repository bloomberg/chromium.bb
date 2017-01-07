// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver/ime_driver_mus.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/ime/ime.mojom.h"
#include "ui/base/ime/ime_bridge.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/ime_driver/input_method_bridge_chromeos.h"
#else
#include "chrome/browser/ui/views/ime_driver/simple_input_method.h"
#endif  // defined(OS_CHROMEOS)

IMEDriver::IMEDriver() {
  ui::IMEBridge::Initialize();
}

IMEDriver::~IMEDriver() {}

// static
void IMEDriver::Register() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ui::mojom::IMEDriverPtr ime_driver_ptr;
  mojo::MakeStrongBinding(base::MakeUnique<IMEDriver>(),
                          MakeRequest(&ime_driver_ptr));
  ui::mojom::IMERegistrarPtr ime_registrar;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ui::mojom::kServiceName, &ime_registrar);
  ime_registrar->RegisterDriver(std::move(ime_driver_ptr));
}

void IMEDriver::StartSession(
    int32_t session_id,
    ui::mojom::TextInputClientPtr client,
    ui::mojom::InputMethodRequest input_method_request) {
#if defined(OS_CHROMEOS)
  input_method_bindings_[session_id] =
      base::MakeUnique<mojo::Binding<ui::mojom::InputMethod>>(
          new InputMethodBridge(std::move(client)),
          std::move(input_method_request));
#else
  input_method_bindings_[session_id] =
      base::MakeUnique<mojo::Binding<ui::mojom::InputMethod>>(
          new SimpleInputMethod());
#endif
}

void IMEDriver::CancelSession(int32_t session_id) {
  input_method_bindings_.erase(session_id);
}
