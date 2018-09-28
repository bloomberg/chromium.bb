// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/android_sms_app_installing_status_observer.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// static
AndroidSmsAppInstallingStatusObserver::Factory*
    AndroidSmsAppInstallingStatusObserver::Factory::test_factory_ = nullptr;

// static
AndroidSmsAppInstallingStatusObserver::Factory*
AndroidSmsAppInstallingStatusObserver::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void AndroidSmsAppInstallingStatusObserver::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

AndroidSmsAppInstallingStatusObserver::Factory::~Factory() = default;

std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
AndroidSmsAppInstallingStatusObserver::Factory::BuildInstance(
    HostStatusProvider* host_status_provider,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate) {
  return base::WrapUnique(new AndroidSmsAppInstallingStatusObserver(
      host_status_provider, std::move(android_sms_app_helper_delegate)));
}

AndroidSmsAppInstallingStatusObserver::
    ~AndroidSmsAppInstallingStatusObserver() {
  host_status_provider_->RemoveObserver(this);
}

AndroidSmsAppInstallingStatusObserver::AndroidSmsAppInstallingStatusObserver(
    HostStatusProvider* host_status_provider,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate)
    : host_status_provider_(host_status_provider),
      android_sms_app_helper_delegate_(
          std::move(android_sms_app_helper_delegate)) {
  host_status_provider_->AddObserver(this);
}

void AndroidSmsAppInstallingStatusObserver::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  mojom::HostStatus status(host_status_with_device.host_status());
  if (status ==
          mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation ||
      status == mojom::HostStatus::kHostVerified) {
    // This call is re-entrant. If the app is already installed, it will just
    // fail silently, which is fine.
    android_sms_app_helper_delegate_->InstallAndroidSmsApp();
  }
}

}  // namespace multidevice_setup

}  // namespace chromeos
