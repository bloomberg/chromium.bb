// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"

#include "ash/resources/grit/ash_resources.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/crx_file/id_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

CrostiniAppModelBuilder::CrostiniAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, CrostiniAppItem::kItemType) {}

CrostiniAppModelBuilder::~CrostiniAppModelBuilder() {
  // We don't need to remove ourself from the registry's observer list as these
  // are both KeyedServices (this class lives on AppListSyncableService).
}

void CrostiniAppModelBuilder::BuildModel() {
  InsertApp(std::make_unique<CrostiniAppItem>(
      profile(), GetSyncItem(kCrostiniTerminalId), kCrostiniTerminalId,
      kCrostiniTerminalAppName,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_CROSTINI_TERMINAL)));

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
  for (const std::string& app_id : registry_service->GetRegisteredAppIds()) {
    InsertCrostiniAppItem(registry_service, app_id);
  }

  registry_service->AddObserver(this);
}

void CrostiniAppModelBuilder::InsertCrostiniAppItem(
    const crostini::CrostiniRegistryService* registry_service,
    const std::string& app_id) {
  std::unique_ptr<crostini::CrostiniRegistryService::Registration>
      registration = registry_service->GetRegistration(app_id);
  DCHECK(registration);
  if (registration->no_display)
    return;
  const std::string& localized_name =
      crostini::CrostiniRegistryService::Registration::Localize(
          registration->name);
  // TODO(timloh): Get an icon from the registry.
  InsertApp(std::make_unique<CrostiniAppItem>(
      profile(), GetSyncItem(app_id), app_id, localized_name,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_CROSTINI_DEFAULT)));
}

void CrostiniAppModelBuilder::OnRegistryUpdated(
    crostini::CrostiniRegistryService* registry_service,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  const bool unsynced_change = false;
  for (const std::string& app_id : removed_apps)
    RemoveApp(app_id, unsynced_change);
  for (const std::string& app_id : updated_apps) {
    RemoveApp(app_id, unsynced_change);
    InsertCrostiniAppItem(registry_service, app_id);
  }
  for (const std::string& app_id : inserted_apps)
    InsertCrostiniAppItem(registry_service, app_id);
}
