// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/ash_dbus_services.h"

#include "ash/dbus/display_service_provider.h"
#include "ash/dbus/liveness_service_provider.h"
#include "ash/dbus/url_handler_service_provider.h"
#include "ash/shell.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "chromeos/dbus/session_manager_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

AshDBusServices::AshDBusServices() {
  // DBusThreadManager is initialized in Chrome or in AshService::InitForMash().
  CHECK(chromeos::DBusThreadManager::IsInitialized());

  dbus::Bus* system_bus =
      chromeos::DBusThreadManager::Get()->IsUsingFakes()
          ? nullptr
          : chromeos::DBusThreadManager::Get()->GetSystemBus();
  display_service_ = chromeos::CrosDBusService::Create(
      system_bus, chromeos::kDisplayServiceName,
      dbus::ObjectPath(chromeos::kDisplayServicePath),
      chromeos::CrosDBusService::CreateServiceProviderList(
          std::make_unique<DisplayServiceProvider>()));
  liveness_service_ = chromeos::CrosDBusService::Create(
      system_bus, chromeos::kLivenessServiceName,
      dbus::ObjectPath(chromeos::kLivenessServicePath),
      chromeos::CrosDBusService::CreateServiceProviderList(
          std::make_unique<LivenessServiceProvider>()));
  url_handler_service_ = chromeos::CrosDBusService::Create(
      system_bus, chromeos::kUrlHandlerServiceName,
      dbus::ObjectPath(chromeos::kUrlHandlerServicePath),
      chromeos::CrosDBusService::CreateServiceProviderList(
          std::make_unique<UrlHandlerServiceProvider>()));
}

void AshDBusServices::EmitAshInitialized() {
  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->EmitAshInitialized();
}

AshDBusServices::~AshDBusServices() {
  display_service_.reset();
  liveness_service_.reset();
  url_handler_service_.reset();
}

}  // namespace ash
