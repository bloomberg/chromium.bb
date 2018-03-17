// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_

#include <memory>

#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace chromeos {

namespace multidevice {

// Concrete MultiDeviceSetup implementation.
class MultiDeviceSetupImpl : public multidevice_setup::mojom::MultiDeviceSetup {
 public:
  MultiDeviceSetupImpl();
  ~MultiDeviceSetupImpl() override;

  // Binds a request to this implementation. Should be called each time that the
  // service receives a request.
  void BindRequest(multidevice_setup::mojom::MultiDeviceSetupRequest request);

  // multidevice_setup::mojom::MultiDeviceSetup:
  void SetObserver(
      multidevice_setup::mojom::MultiDeviceSetupObserverPtr presenter,
      SetObserverCallback callback) override;
  void TriggerEventForDebugging(
      multidevice_setup::mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

 private:
  multidevice_setup::mojom::MultiDeviceSetupObserverPtr observer_;
  mojo::BindingSet<multidevice_setup::mojom::MultiDeviceSetup> bindings_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupImpl);
};

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
