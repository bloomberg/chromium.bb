// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_

#include "base/macros.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

namespace device_sync {

// Fake DeviceSyncObserver implementation for tests.
class FakeDeviceSyncObserver : public device_sync::mojom::DeviceSyncObserver {
 public:
  FakeDeviceSyncObserver();
  ~FakeDeviceSyncObserver() override;

  // Receives callbacks when the relevant DeviceSyncObserver functions have been
  // handled.
  class Delegate {
   public:
    virtual void OnEnrollmentFinishedCalled() = 0;
    virtual void OnDevicesSyncedCalled() = 0;
  };

  void SetDelegate(Delegate* delegate);

  mojom::DeviceSyncObserverPtr GenerateInterfacePtr();

  size_t num_enrollment_success_events() {
    return num_enrollment_success_events_;
  }

  size_t num_enrollment_failure_events() {
    return num_enrollment_failure_events_;
  }

  size_t num_sync_success_events() { return num_sync_success_events_; }

  size_t num_sync_failure_events() { return num_sync_failure_events_; }

  // device_sync::mojom::DeviceSyncObserver:
  void OnEnrollmentFinished(bool success) override;
  void OnDevicesSynced(bool success) override;

 private:
  Delegate* delegate_ = nullptr;

  size_t num_enrollment_success_events_ = 0u;
  size_t num_enrollment_failure_events_ = 0u;
  size_t num_sync_success_events_ = 0u;
  size_t num_sync_failure_events_ = 0u;

  mojo::BindingSet<mojom::DeviceSyncObserver> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceSyncObserver);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_
