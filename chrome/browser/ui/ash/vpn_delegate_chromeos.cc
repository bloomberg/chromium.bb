// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/vpn_delegate_chromeos.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

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

}  // namespace

VPNDelegateChromeOS::VPNDelegateChromeOS() : weak_factory_(this) {
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

VPNDelegateChromeOS::~VPNDelegateChromeOS() {
  if (extension_registry_)
    extension_registry_->RemoveObserver(this);
}

void VPNDelegateChromeOS::ShowAddPage(const std::string& extension_id) {
  if (extension_id.empty()) {
    // Show the "add network" dialog for the built-in OpenVPN/L2TP provider.
    SystemTrayClient::Get()->ShowNetworkCreate(shill::kTypeVPN);
    return;
  }

  Profile* const profile = GetProfileForPrimaryUser();
  if (!profile)
    return;

  // Request that the third-party VPN provider identified by |key.extension_id|
  // show its "add network" dialog.
  chromeos::VpnServiceFactory::GetForBrowserContext(profile)
      ->SendShowAddDialogToExtension(extension_id);
}

void VPNDelegateChromeOS::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (IsVPNProvider(extension))
    UpdateVPNProviders();
}

void VPNDelegateChromeOS::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (IsVPNProvider(extension))
    UpdateVPNProviders();
}

void VPNDelegateChromeOS::OnShutdown(extensions::ExtensionRegistry* registry) {
  DCHECK(extension_registry_);
  extension_registry_->RemoveObserver(this);
  extension_registry_ = nullptr;
}

void VPNDelegateChromeOS::Observe(int type,
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
      base::Bind(&VPNDelegateChromeOS::AttachToPrimaryUserExtensionRegistry,
                 weak_factory_.GetWeakPtr()));
}

void VPNDelegateChromeOS::UpdateVPNProviders() {
  DCHECK(extension_registry_);

  std::vector<ash::VPNProvider> third_party_providers;
  for (const auto& extension : extension_registry_->enabled_extensions()) {
    if (IsVPNProvider(extension.get())) {
      third_party_providers.push_back(
          ash::VPNProvider(extension->id(), extension->name()));
    }
  }

  // Ash adds the built-in OpenVPN/L2TP provider.
  SetThirdPartyVpnProviders(third_party_providers);
}

void VPNDelegateChromeOS::AttachToPrimaryUserExtensionRegistry() {
  DCHECK(!extension_registry_);
  extension_registry_ =
      extensions::ExtensionRegistry::Get(GetProfileForPrimaryUser());
  extension_registry_->AddObserver(this);

  UpdateVPNProviders();
}
