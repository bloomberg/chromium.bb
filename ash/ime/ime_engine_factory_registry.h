// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_ENGINE_FACTORY_REGISTRY_H_
#define ASH_IME_IME_ENGINE_FACTORY_REGISTRY_H_

#include <memory>

#include "ash/ash_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/base/ime/mojo/ime.mojom.h"
#include "ui/base/ime/mojo/ime_engine_factory_registry.mojom.h"

namespace ash {

// This class maintains the active input method engine, as well as forwarding
// requests from clients to the active engine. In particular, engines connect
// using ime::mojom::ImeEngineFactoryRegistry and clients connect using
// ime::mojom::ImeEngineFactory.
class ASH_EXPORT ImeEngineFactoryRegistry
    : public ime::mojom::ImeEngineFactoryRegistry {
 public:
  ImeEngineFactoryRegistry();
  ~ImeEngineFactoryRegistry() override;

  void BindRequest(ime::mojom::ImeEngineFactoryRegistryRequest request);

  void ConnectToImeEngine(ime::mojom::ImeEngineRequest engine_request,
                          ime::mojom::ImeEngineClientPtr client);

  bool HasPendingRequestForTesting() const { return !!pending_engine_request_; }

 private:
  // ime::mojom::ImeEngineFactoryRegistry:
  void ActivateFactory(ime::mojom::ImeEngineFactoryPtr ief) override;

  mojo::BindingSet<ime::mojom::ImeEngineFactoryRegistry> registry_bindings_;

  ime::mojom::ImeEngineFactoryPtr engine_factory_;
  ime::mojom::ImeEngineRequest pending_engine_request_;
  ime::mojom::ImeEngineClientPtr pending_engine_client_;

  DISALLOW_COPY_AND_ASSIGN(ImeEngineFactoryRegistry);
};

}  // namespace ash

#endif  // ASH_IME_IME_ENGINE_FACTORY_REGISTRY_H_
