// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_FACTORY_BASE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_FACTORY_BASE_H_

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/connect_to_device_operation_factory.h"
#include "chromeos/services/secure_channel/device_id_pair.h"

namespace chromeos {

namespace secure_channel {

// ConnectToDeviceOperationFactory implementation, which ensures that only one
// operation can be active at a time.
template <typename FailureDetailType>
class ConnectToDeviceOperationFactoryBase
    : public ConnectToDeviceOperationFactory<FailureDetailType> {
 public:
  ~ConnectToDeviceOperationFactoryBase() override = default;

 protected:
  ConnectToDeviceOperationFactoryBase(const DeviceIdPair& device_id_pair)
      : device_id_pair_(device_id_pair), weak_ptr_factory_(this) {}

  // Derived types should overload this function, passing the provided
  // parameters to the constructor of a type derived from
  // ConnectToDeviceOperationBase.
  virtual std::unique_ptr<ConnectToDeviceOperation<FailureDetailType>>
  PerformCreateOperation(
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionSuccessCallback success_callback,
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionFailedCallback failure_callback,
      const DeviceIdPair& device_id_pair,
      base::OnceClosure destructor_callback) = 0;

 private:
  // ConnectToDeviceOperationFactory<FailureDetailType>:
  std::unique_ptr<ConnectToDeviceOperation<FailureDetailType>> CreateOperation(
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionSuccessCallback success_callback,
      typename ConnectToDeviceOperation<FailureDetailType>::
          ConnectionFailedCallback failure_callback) override {
    if (is_last_operation_active_) {
      PA_LOG(ERROR) << "ConnectToDeviceOperationFactoryBase::CreateOperation():"
                    << " Requested new operation before the previous one was "
                    << "finished.";
      NOTREACHED();
      return nullptr;
    }

    is_last_operation_active_ = true;
    return PerformCreateOperation(
        std::move(success_callback), std::move(failure_callback),
        device_id_pair_,
        base::BindOnce(&ConnectToDeviceOperationFactoryBase<
                           FailureDetailType>::OnPreviousOperationDeleted,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnPreviousOperationDeleted() { is_last_operation_active_ = false; }

  const DeviceIdPair device_id_pair_;
  bool is_last_operation_active_ = false;

  base::WeakPtrFactory<ConnectToDeviceOperationFactoryBase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectToDeviceOperationFactoryBase);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_FACTORY_BASE_H_
