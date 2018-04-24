// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_icon_loader.h"

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/base/resource/resource_bundle.h"

CrostiniAppIconLoader::CrostiniAppIconLoader(Profile* profile,
                                             int resource_size_in_dip,
                                             AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, resource_size_in_dip, delegate),
      profile_(profile) {}

CrostiniAppIconLoader::~CrostiniAppIconLoader() = default;

bool CrostiniAppIconLoader::CanLoadImageForApp(const std::string& app_id) {
  if (icon_map_.find(app_id) != icon_map_.end())
    return true;

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile_);
  return registry_service->IsCrostiniShelfAppId(app_id);
}

void CrostiniAppIconLoader::FetchImage(const std::string& app_id) {
  if (icon_map_.find(app_id) != icon_map_.end())
    return;

  // TODO(timzheng): Use a real specific icon for this app.
  std::unique_ptr<gfx::ImageSkia> image_skia = std::make_unique<gfx::ImageSkia>(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_CROSTINI_DEFAULT));
  icon_map_[app_id] = std::move(image_skia);
  UpdateImage(app_id);
}

void CrostiniAppIconLoader::ClearImage(const std::string& app_id) {
  icon_map_.erase(app_id);
}

void CrostiniAppIconLoader::UpdateImage(const std::string& app_id) {
  AppIDToIconMap::const_iterator it = icon_map_.find(app_id);
  if (it == icon_map_.end())
    return;

  delegate()->OnAppImageUpdated(app_id, *(it->second));
}
