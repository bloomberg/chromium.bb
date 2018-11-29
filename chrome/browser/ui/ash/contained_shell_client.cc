// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/contained_shell_client.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"

ContainedShellClient::ContainedShellClient() {
  ash::mojom::ContainedShellControllerPtr contained_shell_controller;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &contained_shell_controller);

  ash::mojom::ContainedShellClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  contained_shell_controller->SetClient(std::move(client));
}

ContainedShellClient::~ContainedShellClient() = default;
