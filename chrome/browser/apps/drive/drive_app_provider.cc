// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_app_provider.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/drive/drive_app_converter.h"
#include "chrome/browser/apps/drive/drive_app_mapping.h"
#include "chrome/browser/apps/drive/drive_service_bridge.h"
#include "chrome/browser/drive/drive_app_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

void IgnoreUninstallResult(google_apis::GDataErrorCode) {
}

}  // namespace

DriveAppProvider::DriveAppProvider(Profile* profile)
    : profile_(profile),
      service_bridge_(DriveServiceBridge::Create(profile).Pass()),
      mapping_(new DriveAppMapping(profile->GetPrefs())),
      weak_ptr_factory_(this) {
  service_bridge_->GetAppRegistry()->AddObserver(this);
  ExtensionRegistry::Get(profile_)->AddObserver(this);
}

DriveAppProvider::~DriveAppProvider() {
  ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  service_bridge_->GetAppRegistry()->RemoveObserver(this);
}

// static
void DriveAppProvider::AppendDependsOnFactories(
    std::set<BrowserContextKeyedServiceFactory*>* factories) {
  factories->insert(extensions::ExtensionRegistryFactory::GetInstance());
  DriveServiceBridge::AppendDependsOnFactories(factories);
}

void DriveAppProvider::SetDriveServiceBridgeForTest(
    scoped_ptr<DriveServiceBridge> test_bridge) {
  service_bridge_->GetAppRegistry()->RemoveObserver(this);
  service_bridge_ = test_bridge.Pass();
  service_bridge_->GetAppRegistry()->AddObserver(this);
}

void DriveAppProvider::UpdateMappingAndExtensionSystem(
    const std::string& drive_app_id,
    const Extension* new_app,
    bool is_new_app_generated) {
  const std::string& new_chrome_app_id = new_app->id();

  const std::string existing_chrome_app_id =
      mapping_->GetChromeApp(drive_app_id);
  if (existing_chrome_app_id == new_chrome_app_id)
    return;

  const bool is_existing_app_generated =
      mapping_->IsChromeAppGenerated(existing_chrome_app_id);
  mapping_->Add(drive_app_id, new_chrome_app_id, is_new_app_generated);

  const Extension* existing_app =
      ExtensionRegistry::Get(profile_)->GetExtensionById(
          existing_chrome_app_id, ExtensionRegistry::EVERYTHING);
  if (existing_app && is_existing_app_generated) {
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->UninstallExtension(existing_chrome_app_id,
                             extensions::UNINSTALL_REASON_SYNC,
                             base::Bind(&base::DoNothing),
                             NULL);
  }
}

void DriveAppProvider::ProcessDeferredOnExtensionInstalled(
    const std::string drive_app_id,
    const std::string chrome_app_id) {
  const Extension* app = ExtensionRegistry::Get(profile_)->GetExtensionById(
      chrome_app_id, ExtensionRegistry::EVERYTHING);
  if (!app)
    return;

  UpdateMappingAndExtensionSystem(drive_app_id, app, false);
}

void DriveAppProvider::SchedulePendingConverters() {
  if (pending_converters_.empty())
    return;

  if (!pending_converters_.front()->IsStarted())
    pending_converters_.front()->Start();
}

void DriveAppProvider::OnLocalAppConverted(const DriveAppConverter* converter,
                                           bool success) {
  DCHECK_EQ(pending_converters_.front(), converter);

  if (success) {
    const bool was_generated =
        mapping_->GetDriveApp(converter->extension()->id()) ==
            converter->drive_app_info().app_id &&
        mapping_->IsChromeAppGenerated(converter->extension()->id());
    UpdateMappingAndExtensionSystem(
        converter->drive_app_info().app_id,
        converter->extension(),
        converter->is_new_install() || was_generated);
  } else {
    LOG(WARNING) << "Failed to convert drive app to web app, "
                 << "drive app id= " << converter->drive_app_info().app_id
                 << ", name=" << converter->drive_app_info().app_name;
  }

  pending_converters_.erase(pending_converters_.begin());
  SchedulePendingConverters();
}

bool DriveAppProvider::IsMappedUrlAppUpToDate(
    const drive::DriveAppInfo& drive_app) const {
  const std::string& url_app_id = mapping_->GetChromeApp(drive_app.app_id);
  if (url_app_id.empty())
    return false;

  const Extension* url_app = ExtensionRegistry::Get(profile_)->GetExtensionById(
      url_app_id, ExtensionRegistry::EVERYTHING);
  if (!url_app)
    return false;
  DCHECK(url_app->is_hosted_app() && url_app->from_bookmark());

  return drive_app.app_name == url_app->name() &&
         drive_app.create_url ==
             extensions::AppLaunchInfo::GetLaunchWebURL(url_app);
}

void DriveAppProvider::AddOrUpdateDriveApp(
    const drive::DriveAppInfo& drive_app) {
  const Extension* chrome_app =
      ExtensionRegistry::Get(profile_)->GetExtensionById(
          drive_app.product_id, ExtensionRegistry::EVERYTHING);
  if (chrome_app) {
    UpdateMappingAndExtensionSystem(drive_app.app_id, chrome_app, false);
    return;
  }

  if (IsMappedUrlAppUpToDate(drive_app))
    return;

  ScopedVector<DriveAppConverter>::iterator it = pending_converters_.begin();
  while (it != pending_converters_.end()) {
    if (!(*it)->IsStarted() &&
        (*it)->drive_app_info().app_id == drive_app.app_id) {
      it = pending_converters_.erase(it);
    } else {
      ++it;
    }
  }

  pending_converters_.push_back(
      new DriveAppConverter(profile_,
                            drive_app,
                            base::Bind(&DriveAppProvider::OnLocalAppConverted,
                                       base::Unretained(this))));
}

void DriveAppProvider::ProcessRemovedDriveApp(const std::string& drive_app_id) {
  const std::string chrome_app_id = mapping_->GetChromeApp(drive_app_id);
  const bool is_generated = mapping_->IsChromeAppGenerated(chrome_app_id);
  mapping_->Remove(drive_app_id);

  if (chrome_app_id.empty() || !is_generated)
    return;

  const Extension* existing_app =
      ExtensionRegistry::Get(profile_)
          ->GetExtensionById(chrome_app_id, ExtensionRegistry::EVERYTHING);
  if (!existing_app)
    return;

  extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->UninstallExtension(chrome_app_id,
                           extensions::UNINSTALL_REASON_SYNC,
                           base::Bind(&base::DoNothing),
                           NULL);
}

void DriveAppProvider::OnDriveAppRegistryUpdated() {
  service_bridge_->GetAppRegistry()->GetAppList(&drive_apps_);

  IdSet current_ids;
  for (size_t i = 0; i < drive_apps_.size(); ++i)
    current_ids.insert(drive_apps_[i].app_id);

  const IdSet existing_ids = mapping_->GetDriveAppIds();
  const IdSet ids_to_remove =
      base::STLSetDifference<IdSet>(existing_ids, current_ids);
  for (IdSet::const_iterator it = ids_to_remove.begin();
       it != ids_to_remove.end();
       ++it) {
    ProcessRemovedDriveApp(*it);
  }

  for (size_t i = 0; i < drive_apps_.size(); ++i) {
    AddOrUpdateDriveApp(drive_apps_[i]);
  }
  SchedulePendingConverters();
}

void DriveAppProvider::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  // Bail if the |extension| is installed from a converter. The post install
  // processing will be handled in OnLocalAppConverted.
  if (!pending_converters_.empty() &&
      pending_converters_.front()->IsInstalling(extension->id())) {
    return;
  }

  // Only user installed app reaches here. If it is mapped, make sure it is not
  // tagged as generated.
  const std::string drive_app_id = mapping_->GetDriveApp(extension->id());
  if (!drive_app_id.empty() &&
      mapping_->IsChromeAppGenerated(extension->id())) {
    mapping_->Add(drive_app_id, extension->id(), false);
    return;
  }

  for (size_t i = 0; i < drive_apps_.size(); ++i) {
    if (drive_apps_[i].product_id == extension->id()) {
      // Defer the processing because it touches the extensions system and
      // it is better to let the current task finish to avoid unexpected
      // incomplete status.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&DriveAppProvider::ProcessDeferredOnExtensionInstalled,
                     weak_ptr_factory_.GetWeakPtr(),
                     drive_apps_[i].app_id,
                     extension->id()));
      return;
    }
  }
}

void DriveAppProvider::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  std::string drive_app_id = mapping_->GetDriveApp(extension->id());
  if (drive_app_id.empty())
    return;

  service_bridge_->GetAppRegistry()->UninstallApp(
      drive_app_id, base::Bind(&IgnoreUninstallResult));
}
