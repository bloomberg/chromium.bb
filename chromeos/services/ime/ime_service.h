// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_IME_SERVICE_H_
#define CHROMEOS_SERVICES_IME_IME_SERVICE_H_

#include "chromeos/services/ime/input_engine.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace chromeos {
namespace ime {

std::unique_ptr<service_manager::Service> CreateImeService();

class ImeService : public service_manager::Service,
                   public mojom::InputEngineManager {
 public:
  ImeService();
  ~ImeService() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::InputEngineManager overrides:
  void ConnectToImeEngine(const std::string& ime_spec,
                          mojom::InputChannelRequest to_engine_request,
                          mojom::InputChannelPtr from_engine,
                          const std::vector<uint8_t>& extra,
                          ConnectToImeEngineCallback callback) override;

  // Binds the mojom::InputEngineManager interface to this object.
  void BindInputEngineManagerRequest(mojom::InputEngineManagerRequest request);

  // For the duration of this service lifetime, there should be only one
  // input engine instance.
  std::unique_ptr<InputEngine> input_engine_;

  mojo::BindingSet<mojom::InputEngineManager> engine_manager_bindings_;

  service_manager::BinderRegistry binder_registry_;

  DISALLOW_COPY_AND_ASSIGN(ImeService);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_IME_SERVICE_H_
