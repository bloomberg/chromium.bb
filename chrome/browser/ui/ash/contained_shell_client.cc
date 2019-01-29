// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/contained_shell_client.h"

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

void ContainedShellClient::LaunchContainedShell(const AccountId& account_id) {
  // TODO(michaelpg): Create a dummy app for non-internal builds.

#if defined(GOOGLE_CHROME_BUILD)
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          extension_misc::kContainedHomeAppId);
  DCHECK(app);
  apps::LaunchPlatformApp(profile, app,
                          extensions::AppLaunchSource::SOURCE_CHROME_INTERNAL);
#endif
}
