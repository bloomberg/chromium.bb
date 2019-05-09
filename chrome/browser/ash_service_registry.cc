// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash_service_registry.h"

#include "ash/ash_service.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/prefs/pref_connector_service.h"
#include "content/public/common/service_manager_connection.h"

namespace ash_service_registry {

std::unique_ptr<service_manager::Service> HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == ash::mojom::kPrefConnectorServiceName)
    return std::make_unique<AshPrefConnector>(std::move(request));
  return service_name == ash::mojom::kServiceName
             ? std::make_unique<ash::AshService>(std::move(request))
             : nullptr;
}

}  // namespace ash_service_registry
