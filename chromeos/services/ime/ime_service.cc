// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include "base/bind.h"
#include "build/buildflag.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"

#if BUILDFLAG(ENABLE_CROS_IME_DECODER)
#include "chromeos/services/ime/decoder/decoder_engine.h"
#include "services/service_manager/public/cpp/service_context.h"
#endif

namespace chromeos {
namespace ime {

std::unique_ptr<service_manager::Service> CreateImeService() {
  return std::make_unique<ImeService>();
}

ImeService::ImeService() {}

ImeService::~ImeService() {}

void ImeService::OnStart() {
  binder_registry_.AddInterface<mojom::InputEngineManager>(base::BindRepeating(
      &ImeService::BindInputEngineManagerRequest, base::Unretained(this)));

#if BUILDFLAG(ENABLE_CROS_IME_DECODER)
  input_engine_ = std::make_unique<DecoderEngine>(
      context()->connector(), base::SequencedTaskRunnerHandle::Get());
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
  input_engine_->BindRequest(ime_spec, std::move(to_engine_request),
                             std::move(from_engine), extra);
}

void ImeService::BindInputEngineManagerRequest(
    mojom::InputEngineManagerRequest request) {
  engine_manager_bindings_.AddBinding(this, std::move(request));
  // TODO(https://crbug.com/837156): Registry connection error handler.
}

}  // namespace ime
}  // namespace chromeos
