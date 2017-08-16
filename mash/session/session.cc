// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/session/session.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mash/common/config.h"
#include "mash/quick_launch/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace mash {
namespace session {

Session::Session() {}
Session::~Session() {}

void Session::OnStart() {
  StartWindowManager();
  // TODO(jonross): Re-enable when QuickLaunch for all builds once it no longer
  // deadlocks with ServiceManager shutdown in mash_browser_tests.
  // (crbug.com/594852)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          quick_launch::mojom::kServiceName)) {
    StartQuickLaunch();
  }
}

void Session::StartWindowManager() {
  // TODO(beng): monitor this service for death & bring down the whole system
  // if necessary.
  context()->connector()->StartService(common::GetWindowManagerServiceName());
}

void Session::StartQuickLaunch() {
  // TODO(beng): monitor this service for death & bring down the whole system
  // if necessary.
  context()->connector()->StartService(quick_launch::mojom::kServiceName);
}

}  // namespace session
}  // namespace main
