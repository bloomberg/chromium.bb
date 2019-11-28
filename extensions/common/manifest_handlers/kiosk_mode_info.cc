// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

#include <memory>
#include <set>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;

using api::extensions_manifest_types::KioskSecondaryAppsType;

namespace {

// Whether "enabled_on_launch" manifest property for the extension should be
// respected or not. If false, secondary apps that specify this property will
// be ignored.
bool AllowSecondaryAppEnabledOnLaunch(const Extension* extension) {
  if (!extension)
    return false;

  const Feature* feature = FeatureProvider::GetBehaviorFeatures()->GetFeature(
      behavior_feature::kAllowSecondaryKioskAppEnabledOnLaunch);
  if (!feature)
    return false;

  return feature->IsAvailableToExtension(extension).is_available();
}

}  // namespace

SecondaryKioskAppInfo::SecondaryKioskAppInfo(
    const extensions::ExtensionId& id,
    const base::Optional<bool>& enabled_on_launch)
    : id(id), enabled_on_launch(enabled_on_launch) {}

SecondaryKioskAppInfo::SecondaryKioskAppInfo(
    const SecondaryKioskAppInfo& other) = default;

SecondaryKioskAppInfo::~SecondaryKioskAppInfo() = default;

KioskModeInfo::KioskModeInfo(
    KioskStatus kiosk_status,
    std::vector<SecondaryKioskAppInfo>&& secondary_apps,
    const std::string& required_platform_version,
    bool always_update)
    : kiosk_status(kiosk_status),
      secondary_apps(std::move(secondary_apps)),
      required_platform_version(required_platform_version),
      always_update(always_update) {}

KioskModeInfo::~KioskModeInfo() {
}

// static
KioskModeInfo* KioskModeInfo::Get(const Extension* extension) {
  return static_cast<KioskModeInfo*>(
      extension->GetManifestData(keys::kKioskMode));
}

// static
bool KioskModeInfo::IsKioskEnabled(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info ? info->kiosk_status != NONE : false;
}

// static
bool KioskModeInfo::IsKioskOnly(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info ? info->kiosk_status == ONLY : false;
}

// static
bool KioskModeInfo::HasSecondaryApps(const Extension* extension) {
  KioskModeInfo* info = Get(extension);
  return info && !info->secondary_apps.empty();
}

// static
bool KioskModeInfo::IsValidPlatformVersion(const std::string& version_string) {
  const base::Version version(version_string);
  return version.IsValid() && version.components().size() <= 3u;
}

KioskModeHandler::KioskModeHandler() {
}

KioskModeHandler::~KioskModeHandler() {
}

bool KioskModeHandler::Parse(Extension* extension, base::string16* error) {
  const Manifest* manifest = extension->manifest();
  DCHECK(manifest->HasKey(keys::kKioskEnabled) ||
         manifest->HasKey(keys::kKioskOnly));

  bool kiosk_enabled = false;
  if (manifest->HasKey(keys::kKioskEnabled) &&
      !manifest->GetBoolean(keys::kKioskEnabled, &kiosk_enabled)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskEnabled);
    return false;
  }

  bool kiosk_only = false;
  if (manifest->HasKey(keys::kKioskOnly) &&
      !manifest->GetBoolean(keys::kKioskOnly, &kiosk_only)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskOnly);
    return false;
  }

  if (kiosk_only && !kiosk_enabled) {
    *error = base::ASCIIToUTF16(
        manifest_errors::kInvalidKioskOnlyButNotEnabled);
    return false;
  }

  // All other use cases should be already filtered out by manifest feature
  // checks.
  DCHECK(extension->is_platform_app());

  KioskModeInfo::KioskStatus kiosk_status = KioskModeInfo::NONE;
  if (kiosk_enabled)
    kiosk_status = kiosk_only ? KioskModeInfo::ONLY : KioskModeInfo::ENABLED;

  // Kiosk secondary apps key is optional.
  std::vector<SecondaryKioskAppInfo> secondary_apps;
  std::set<std::string> secondary_app_ids;
  if (manifest->HasKey(keys::kKioskSecondaryApps)) {
    const base::Value* secondary_apps_value = nullptr;
    if (!manifest->GetList(keys::kKioskSecondaryApps, &secondary_apps_value)) {
      *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskSecondaryApps);
      return false;
    }

    const bool allow_enabled_on_launch =
        AllowSecondaryAppEnabledOnLaunch(extension);

    for (const auto& value : secondary_apps_value->GetList()) {
      std::unique_ptr<KioskSecondaryAppsType> app =
          KioskSecondaryAppsType::FromValue(value, error);
      if (!app) {
        *error = base::ASCIIToUTF16(
            manifest_errors::kInvalidKioskSecondaryAppsBadAppEntry);
        return false;
      }

      if (secondary_app_ids.count(app->id)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            manifest_errors::kInvalidKioskSecondaryAppsDuplicateApp, app->id);
        return false;
      }

      if (app->enabled_on_launch && !allow_enabled_on_launch) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            manifest_errors::kInvalidKioskSecondaryAppsPropertyUnavailable,
            "enabled_on_launch", app->id);
        return false;
      }

      base::Optional<bool> enabled_on_launch;
      if (app->enabled_on_launch)
        enabled_on_launch = *app->enabled_on_launch;

      secondary_apps.emplace_back(app->id, enabled_on_launch);
      secondary_app_ids.insert(app->id);
    }
  }

  // Optional kiosk.required_platform_version key.
  std::string required_platform_version;
  if (manifest->HasPath(keys::kKioskRequiredPlatformVersion) &&
      (!manifest->GetString(keys::kKioskRequiredPlatformVersion,
                            &required_platform_version) ||
       !KioskModeInfo::IsValidPlatformVersion(required_platform_version))) {
    *error = base::ASCIIToUTF16(
        manifest_errors::kInvalidKioskRequiredPlatformVersion);
    return false;
  }

  // Optional kiosk.always_update key.
  bool always_update = false;
  if (manifest->HasPath(keys::kKioskAlwaysUpdate) &&
      !manifest->GetBoolean(keys::kKioskAlwaysUpdate, &always_update)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidKioskAlwaysUpdate);
    return false;
  }

  extension->SetManifestData(keys::kKioskMode,
                             std::make_unique<KioskModeInfo>(
                                 kiosk_status, std::move(secondary_apps),
                                 required_platform_version, always_update));

  return true;
}

base::span<const char* const> KioskModeHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kKiosk, keys::kKioskEnabled,
                                          keys::kKioskOnly,
                                          keys::kKioskSecondaryApps};
  return kKeys;
}

}  // namespace extensions
