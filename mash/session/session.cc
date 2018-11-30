// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/session/session.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "mash/common/config.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_CHROMEOS)
#include "ash/components/quick_launch/public/mojom/constants.mojom.h"  // nogncheck
#endif

namespace mash {
namespace session {

Session::Session(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

Session::~Session() = default;

void Session::OnStart() {
  StartWindowManager();
#if defined(OS_CHROMEOS)
  // TODO(jonross): Re-enable when QuickLaunch for all builds once it no longer
  // deadlocks with ServiceManager shutdown in mash_browser_tests.
  // (crbug.com/594852)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          quick_launch::mojom::kServiceName)) {
    // TODO(https://crbug.com/904148): This should not use |WarmService()|.
    service_binding_.GetConnector()->WarmService(
        service_manager::ServiceFilter::ByName(
            quick_launch::mojom::kServiceName));
  }
#endif  // defined(OS_CHROMEOS)
}

void Session::StartWindowManager() {
  // TODO(beng): monitor this service for death & bring down the whole system
  // if necessary.
  service_binding_.GetConnector()->WarmService(
      service_manager::ServiceFilter::ByName(
          common::GetWindowManagerServiceName()));
}

}  // namespace session
}  // namespace main
