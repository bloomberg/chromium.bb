// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_service.h"

#include "components/startup_metric_utils/browser/startup_metric_host_impl.h"

// static
std::unique_ptr<service_manager::Service> ChromeService::Create() {
  return base::MakeUnique<ChromeService>();
}

ChromeService::ChromeService() {
#if defined(USE_OZONE)
  input_device_controller_.AddInterface(&registry_);
#endif
#if defined(OS_CHROMEOS)
  registry_.AddInterface(
      base::Bind(&chromeos::Launchable::Bind, base::Unretained(&launchable_)));
#endif
  registry_.AddInterface(
      base::Bind(&startup_metric_utils::StartupMetricHostImpl::Create));
}

ChromeService::~ChromeService() {}

void ChromeService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& name,
    mojo::ScopedMessagePipeHandle handle) {
  registry_.TryBindInterface(name, &handle);
}
