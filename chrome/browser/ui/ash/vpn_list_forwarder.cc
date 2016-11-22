// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/vpn_list_forwarder.h"

#include "ash/public/interfaces/vpn_list.mojom.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

bool IsVPNProvider(const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      extensions::APIPermission::kVpnProvider);
}

Profile* GetProfileForPrimaryUser() {
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;

  return chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
}

// Connects to the VpnList mojo interface in ash.
ash::mojom::VpnListPtr ConnectToVpnList() {
  ash::mojom::VpnListPtr vpn_list;
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  // Under mash the VpnList interface is in the ash process. In classic ash
  // we provide it to ourself.
  if (chrome::IsRunningInMash())
    connector->ConnectToInterface("ash", &vpn_list);
  else
    connector->ConnectToInterface("content_browser", &vpn_list);
  return vpn_list;
}

}  // namespace

VpnListForwarder::VpnListForwarder() : weak_factory_(this) {
  if (user_manager::UserManager::Get()->GetPrimaryUser()) {
    // If a user is logged in, start observing the primary user's extension
    // registry immediately.
    AttachToPrimaryUserExtensionRegistry();
  } else {
    // If no user is logged in, wait until the first user logs in (thus becoming
    // the primary user) and a profile is created for that user.
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                   content::NotificationService::AllSources());
  }
}

VpnListForwarder::~VpnListForwarder() {
  if (extension_registry_)
    extension_registry_->RemoveObserver(this);
}

void VpnListForwarder::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (IsVPNProvider(extension))
    UpdateVPNProviders();
}

void VpnListForwarder::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (IsVPNProvider(extension))
    UpdateVPNProviders();
}

void VpnListForwarder::OnShutdown(extensions::ExtensionRegistry* registry) {
  DCHECK(extension_registry_);
  extension_registry_->RemoveObserver(this);
  extension_registry_ = nullptr;
}

void VpnListForwarder::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CREATED, type);
  const Profile* const profile = content::Source<Profile>(source).ptr();
  if (!chromeos::ProfileHelper::Get()->IsPrimaryProfile(profile)) {
    // If the profile that was just created does not belong to the primary user
    // (e.g. login profile), ignore it.
    return;
  }

  // The first user logged in (thus becoming the primary user) and a profile was
  // created for that user. Stop observing profile creation. Wait one message
  // loop cycle to allow other code which observes the
  // chrome::NOTIFICATION_PROFILE_CREATED notification to finish initializing
  // the profile, then start observing the primary user's extension registry.
  registrar_.RemoveAll();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&VpnListForwarder::AttachToPrimaryUserExtensionRegistry,
                 weak_factory_.GetWeakPtr()));
}

void VpnListForwarder::UpdateVPNProviders() {
  DCHECK(extension_registry_);

  std::vector<ash::mojom::ThirdPartyVpnProviderPtr> third_party_providers;
  for (const auto& extension : extension_registry_->enabled_extensions()) {
    if (!IsVPNProvider(extension.get()))
      continue;

    ash::mojom::ThirdPartyVpnProviderPtr provider =
        ash::mojom::ThirdPartyVpnProvider::New();
    provider->name = extension->name();
    provider->extension_id = extension->id();
    third_party_providers.push_back(std::move(provider));
  }

  // Ash starts without any third-party providers. If we've never sent one then
  // there's no need to send an empty list. This case commonly occurs on startup
  // when the user has no third-party VPN extensions installed.
  if (!sent_providers_ && third_party_providers.empty())
    return;

  // It's rare to install or uninstall VPN provider extensions, so don't bother
  // caching the interface pointer between calls to this function.
  ash::mojom::VpnListPtr vpn_list = ConnectToVpnList();
  vpn_list->SetThirdPartyVpnProviders(std::move(third_party_providers));

  sent_providers_ = true;
}

void VpnListForwarder::AttachToPrimaryUserExtensionRegistry() {
  DCHECK(!extension_registry_);
  extension_registry_ =
      extensions::ExtensionRegistry::Get(GetProfileForPrimaryUser());
  extension_registry_->AddObserver(this);

  UpdateVPNProviders();
}
