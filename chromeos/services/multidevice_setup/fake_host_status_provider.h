// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_PROVIDER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"

namespace chromeos {

namespace multidevice_setup {

// Test HostStatusProvider implementation.
class FakeHostStatusProvider : public HostStatusProvider {
 public:
  FakeHostStatusProvider();
  ~FakeHostStatusProvider() override;

  void SetHostWithStatus(
      mojom::HostStatus host_status,
      const base::Optional<multidevice::RemoteDeviceRef>& host_device);

  // HostStatusProvider:
  HostStatusWithDevice GetHostWithStatus() const override;

 private:
  mojom::HostStatus host_status_ = mojom::HostStatus::kNoEligibleHosts;
  base::Optional<multidevice::RemoteDeviceRef> host_device_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostStatusProvider);
};

// Test HostStatusProvider::Observer implementation.
class FakeHostStatusProviderObserver : public HostStatusProvider::Observer {
 public:
  FakeHostStatusProviderObserver();
  ~FakeHostStatusProviderObserver() override;

  const std::vector<HostStatusProvider::HostStatusWithDevice>&
  host_status_updates() const {
    return host_status_updates_;
  }

 private:
  // HostStatusProvider::Observer:
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  std::vector<HostStatusProvider::HostStatusWithDevice> host_status_updates_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostStatusProviderObserver);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_STATUS_PROVIDER_H_
