// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTIDEVICE_SERVICE_DEVICE_SYNC_IMPL_H_
#define COMPONENTS_MULTIDEVICE_SERVICE_DEVICE_SYNC_IMPL_H_

#include "base/macros.h"
#include "components/multidevice/service/public/interfaces/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace device_sync {

// Concrete DeviceSync implementation.
class DeviceSyncImpl : public mojom::DeviceSync {
 public:
  DeviceSyncImpl();
  ~DeviceSyncImpl() override;

  // Binds a request to this implementation. Should be called each time that the
  // service receives a request.
  void BindRequest(mojom::DeviceSyncRequest request);

  // mojom::DeviceSync:
  void ForceEnrollmentNow() override;
  void ForceSyncNow() override;
  void AddObserver(mojom::DeviceSyncObserverPtr observer,
                   AddObserverCallback callback) override;

 private:
  mojo::InterfacePtrSet<mojom::DeviceSyncObserver> observers_;
  mojo::BindingSet<mojom::DeviceSync> bindings_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncImpl);
};

}  // namespace device_sync

#endif  // COMPONENTS_MULTIDEVICE_SERVICE_DEVICE_SYNC_IMPL_H_
