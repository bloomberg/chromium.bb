// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/apk_web_app_service.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/apps/apk_web_app_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/connection_holder.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// The pref dict is:
// {
//  ...
//  "web_app_apks" : {
//    <extension_id_1> : {
//      "package_name" : <apk_package_name_1>,
//    },
//    <extension_id_2> : {
//      "package_name" : <apk_package_name_2>,
//    },
//    ...
//  },
//  ...
// }
const char kWebAppToApkDictPref[] = "web_app_apks";
const char kPackageNameKey[] = "package_name";

// Default icon size in pixels to request from ARC for an icon.
const int kDefaultIconSize = 192;

}  // namespace

namespace chromeos {

// static
ApkWebAppService* ApkWebAppService::Get(Profile* profile) {
  return ApkWebAppServiceFactory::GetForProfile(profile);
}

// static
void ApkWebAppService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kWebAppToApkDictPref);
}

ApkWebAppService::ApkWebAppService(Profile* profile)
    : profile_(profile),
      arc_app_list_prefs_(ArcAppListPrefs::Get(profile)),
      weak_ptr_factory_(this) {
  // Can be null in tests.
  if (arc_app_list_prefs_)
    arc_app_list_prefs_->AddObserver(this);
}

ApkWebAppService::~ApkWebAppService() = default;

void ApkWebAppService::Shutdown() {
  // Can be null in tests.
  if (arc_app_list_prefs_) {
    arc_app_list_prefs_->RemoveObserver(this);
    arc_app_list_prefs_ = nullptr;
  }
}

void ApkWebAppService::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  if (package_info.web_app_info.is_null())
    return;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs_->app_connection_holder(), RequestPackageIcon);
  if (!instance)
    return;

  instance->RequestPackageIcon(
      package_info.package_name, kDefaultIconSize, /*normalize=*/false,
      base::BindOnce(&ApkWebAppService::OnDidGetWebAppIcon,
                     weak_ptr_factory_.GetWeakPtr(), package_info.package_name,
                     package_info.web_app_info.Clone()));
}

void ApkWebAppService::OnDidGetWebAppIcon(
    const std::string& package_name,
    arc::mojom::WebAppInfoPtr web_app_info,
    const std::vector<uint8_t>& icon_png_data) {
  ApkWebAppInstaller::Install(
      profile_, std::move(web_app_info), icon_png_data,
      base::BindOnce(&ApkWebAppService::OnDidFinishInstall,
                     weak_ptr_factory_.GetWeakPtr(), package_name),
      weak_ptr_factory_.GetWeakPtr());
}

void ApkWebAppService::OnDidFinishInstall(
    const std::string& package_name,
    const extensions::ExtensionId& web_app_id) {
  DictionaryPrefUpdate dict_update(profile_->GetPrefs(), kWebAppToApkDictPref);
  dict_update->SetPath({web_app_id, kPackageNameKey},
                       base::Value(package_name));
}

}  // namespace chromeos
