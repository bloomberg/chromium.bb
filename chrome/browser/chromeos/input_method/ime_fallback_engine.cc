// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ime_fallback_engine.h"

#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

ImeFallbackEngine::ImeFallbackEngine()
    : factory_binding_(this), engine_binding_(this) {}

ImeFallbackEngine::~ImeFallbackEngine() = default;

void ImeFallbackEngine::Activate() {
  ime::mojom::ImeEngineFactoryPtr factory_ptr;
  factory_binding_.Close();
  factory_binding_.Bind(mojo::MakeRequest(&factory_ptr));
  factory_binding_.set_connection_error_handler(base::BindOnce(
      &ImeFallbackEngine::OnFactoryConnectionLost, base::Unretained(this)));

  auto* conn = content::ServiceManagerConnection::GetForProcess();
  conn->GetConnector()->BindInterface(ash::mojom::kServiceName, &registry_);
  registry_->ActivateFactory(std::move(factory_ptr));
}

void ImeFallbackEngine::CreateEngine(
    ime::mojom::ImeEngineRequest engine_request,
    ime::mojom::ImeEngineClientPtr client) {
  engine_binding_.Close();
  engine_binding_.Bind(std::move(engine_request));
  engine_client_ = std::move(client);
}

void ImeFallbackEngine::ProcessKeyEvent(
    std::unique_ptr<ui::Event> key_event,
    ime::mojom::ImeEngine::ProcessKeyEventCallback cb) {
  std::move(cb).Run(false);
}

void ImeFallbackEngine::OnFactoryConnectionLost() {
  if (engine_client_)
    engine_client_->Reconnect();
}

}  // namespace chromeos
