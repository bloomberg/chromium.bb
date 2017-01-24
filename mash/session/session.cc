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
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace {

void LogAndCallServiceRestartCallback(const std::string& url,
                                      const base::Closure& callback) {
  LOG(ERROR) << "Restarting service: " << url;
  callback.Run();
}

}  // namespace

namespace mash {
namespace session {

Session::Session() {}
Session::~Session() {}

void Session::OnStart() {
  StartWindowManager();
  StartQuickLaunch();

  // Launch a chrome window for dev convience; don't do this in the long term.
  context()->connector()->Connect(content::mojom::kBrowserServiceName);
}

void Session::StartWindowManager() {
  StartRestartableService(
      common::GetWindowManagerServiceName(),
      base::Bind(&Session::StartWindowManager,
                 base::Unretained(this)));
}

void Session::StartQuickLaunch() {
  StartRestartableService(
      quick_launch::mojom::kServiceName,
      base::Bind(&Session::StartQuickLaunch,
                 base::Unretained(this)));
}

void Session::StartRestartableService(
    const std::string& url,
    const base::Closure& restart_callback) {
  // TODO(beng): This would be the place to insert logic that counted restarts
  //             to avoid infinite crash-restart loops.
  std::unique_ptr<service_manager::Connection> connection =
      context()->connector()->Connect(url);
  // Note: |connection| may be null if we've lost our connection to the service
  // manager.
  if (connection) {
    connection->SetConnectionLostClosure(
        base::Bind(&LogAndCallServiceRestartCallback, url, restart_callback));
    connections_[url] = std::move(connection);
  }
}

}  // namespace session
}  // namespace main
