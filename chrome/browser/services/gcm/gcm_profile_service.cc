// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_driver.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/identity_provider.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#endif

namespace gcm {

// static
GCMProfileService::GCMEnabledState GCMProfileService::GetGCMEnabledState(
    Profile* profile) {
  const base::Value* gcm_enabled_value =
      profile->GetPrefs()->GetUserPrefValue(prefs::kGCMChannelEnabled);
  if (!gcm_enabled_value)
    return ENABLED_FOR_APPS;

  bool gcm_enabled = false;
  if (!gcm_enabled_value->GetAsBoolean(&gcm_enabled))
    return ENABLED_FOR_APPS;

  return gcm_enabled ? ALWAYS_ENABLED : ALWAYS_DISABLED;
}

// static
std::string GCMProfileService::GetGCMEnabledStateString(GCMEnabledState state) {
  switch (state) {
    case GCMProfileService::ALWAYS_ENABLED:
      return "ALWAYS_ENABLED";
    case GCMProfileService::ENABLED_FOR_APPS:
      return "ENABLED_FOR_APPS";
    case GCMProfileService::ALWAYS_DISABLED:
      return "ALWAYS_DISABLED";
    default:
      NOTREACHED();
      return std::string();
  }
}

// static
void GCMProfileService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // GCM support is only enabled by default for Canary/Dev/Custom builds.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  bool on_by_default = false;
  if (channel == chrome::VersionInfo::CHANNEL_UNKNOWN ||
      channel == chrome::VersionInfo::CHANNEL_CANARY ||
      channel == chrome::VersionInfo::CHANNEL_DEV) {
    on_by_default = true;
  }
  registry->RegisterBooleanPref(
      prefs::kGCMChannelEnabled,
      on_by_default,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

GCMProfileService::GCMProfileService(
    Profile* profile,
    scoped_ptr<GCMClientFactory> gcm_client_factory)
    : profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());

#if defined(OS_ANDROID)
  LoginUIService* login_ui_service = NULL;
#else
  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(profile_);
#endif
  driver_.reset(new GCMDriver(
      gcm_client_factory.Pass(),
      scoped_ptr<IdentityProvider>(new ProfileIdentityProvider(
          SigninManagerFactory::GetForProfile(profile_),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
          login_ui_service)),
      profile_->GetPath().Append(chrome::kGCMStoreDirname),
      profile_->GetRequestContext()));
}

GCMProfileService::GCMProfileService() : profile_(NULL) {
}

GCMProfileService::~GCMProfileService() {
}

void GCMProfileService::AddAppHandler(const std::string& app_id,
                                      GCMAppHandler* handler) {
  if (driver_)
    driver_->AddAppHandler(app_id, handler);
}

void GCMProfileService::RemoveAppHandler(const std::string& app_id) {
  if (driver_)
    driver_->RemoveAppHandler(app_id);
}

void GCMProfileService::Register(const std::string& app_id,
                                 const std::vector<std::string>& sender_ids,
                                 const GCMDriver::RegisterCallback& callback) {
  if (driver_)
    driver_->Register(app_id, sender_ids, callback);
}

void GCMProfileService::Shutdown() {
  if (driver_) {
    driver_->Shutdown();
    driver_.reset();
  }
}

void GCMProfileService::SetDriverForTesting(GCMDriver* driver) {
  driver_.reset(driver);
}

}  // namespace gcm
