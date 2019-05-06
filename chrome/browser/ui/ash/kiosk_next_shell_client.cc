// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/kiosk_next_shell_client.h"

#include <utility>

#include "apps/launcher.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

KioskNextShellClient* g_kiosk_next_shell_client_instance = nullptr;

}  // namespace

KioskNextShellClient::KioskNextShellClient() {
  DCHECK(!g_kiosk_next_shell_client_instance);
  g_kiosk_next_shell_client_instance = this;

  ash::mojom::KioskNextShellControllerPtr kiosk_next_shell_controller;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &kiosk_next_shell_controller);

  ash::mojom::KioskNextShellClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  kiosk_next_shell_controller->SetClient(std::move(client));
}

KioskNextShellClient::~KioskNextShellClient() {
  DCHECK_EQ(this, g_kiosk_next_shell_client_instance);
  g_kiosk_next_shell_client_instance = nullptr;
}

// static
KioskNextShellClient* KioskNextShellClient::Get() {
  return g_kiosk_next_shell_client_instance;
}

void KioskNextShellClient::LaunchKioskNextShell(const AccountId& account_id) {
  has_launched_ = true;
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          extension_misc::kKioskNextHomeAppId);
  DCHECK(app);
  apps::LaunchPlatformApp(profile, app,
                          extensions::AppLaunchSource::SOURCE_CHROME_INTERNAL);
}
