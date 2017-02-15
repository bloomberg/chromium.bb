// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/session/session.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "content/public/common/service_names.mojom.h"
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
  StartQuickLaunch();

  // Launch a chrome window for dev convience; don't do this in the long term.
  context()->connector()->Connect(content::mojom::kPackagedServicesServiceName);
}

void Session::StartWindowManager() {
  // TODO(beng): monitor this service for death & bring down the whole system
  // if necessary.
  context()->connector()->Connect(common::GetWindowManagerServiceName());
}

void Session::StartQuickLaunch() {
  // TODO(beng): monitor this service for death & bring down the whole system
  // if necessary.
  context()->connector()->Connect(quick_launch::mojom::kServiceName);
}

}  // namespace session
}  // namespace main
