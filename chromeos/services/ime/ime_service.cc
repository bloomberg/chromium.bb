// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include "base/bind.h"
#include "build/buildflag.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"

#if BUILDFLAG(ENABLE_CROS_IME_DECODER)
#include "chromeos/services/ime/decoder/decoder_engine.h"
#endif

namespace chromeos {
namespace ime {

ImeService::ImeService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

ImeService::~ImeService() = default;

void ImeService::OnStart() {
  binder_registry_.AddInterface<mojom::InputEngineManager>(base::BindRepeating(
      &ImeService::BindInputEngineManagerRequest, base::Unretained(this)));

  engine_manager_bindings_.set_connection_error_handler(base::BindRepeating(
      &ImeService::OnConnectionLost, base::Unretained(this)));

#if BUILDFLAG(ENABLE_CROS_IME_DECODER)
  input_engine_ = std::make_unique<DecoderEngine>(
      service_binding_.GetConnector(), base::SequencedTaskRunnerHandle::Get());
#else
  input_engine_ = std::make_unique<InputEngine>();
#endif
}

void ImeService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  binder_registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void ImeService::ConnectToImeEngine(
    const std::string& ime_spec,
    mojom::InputChannelRequest to_engine_request,
    mojom::InputChannelPtr from_engine,
    const std::vector<uint8_t>& extra,
    ConnectToImeEngineCallback callback) {
  DCHECK(input_engine_);
  bool bound = input_engine_->BindRequest(
      ime_spec, std::move(to_engine_request), std::move(from_engine), extra);
  std::move(callback).Run(bound);
}

void ImeService::BindInputEngineManagerRequest(
    mojom::InputEngineManagerRequest request) {
  engine_manager_bindings_.AddBinding(this, std::move(request));
  // TODO(https://crbug.com/837156): Reset the cleanup timer.
}

void ImeService::OnConnectionLost() {
  if (engine_manager_bindings_.empty()) {
    // TODO(https://crbug.com/837156): Set a timer to start a cleanup.
  }
}

}  // namespace ime
}  // namespace chromeos
