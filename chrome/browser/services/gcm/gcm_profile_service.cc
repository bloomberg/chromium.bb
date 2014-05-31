// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

#if defined(OS_ANDROID)
#include "components/gcm_driver/gcm_driver_android.h"
#else
#include "base/files/file_path.h"
#include "chrome/browser/services/gcm/gcm_utils.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/identity_provider.h"
#include "net/url_request/url_request_context_getter.h"
#endif

namespace gcm {

// static
bool GCMProfileService::IsGCMEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kGCMChannelEnabled);
}

// static
void GCMProfileService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kGCMChannelEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

GCMProfileService::GCMProfileService(
    Profile* profile,
    scoped_ptr<GCMClientFactory> gcm_client_factory)
    : profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());

#if defined(OS_ANDROID)
  driver_.reset(new GCMDriverAndroid());
#else
  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(profile_);
  scoped_refptr<base::SequencedWorkerPool> worker_pool(
      content::BrowserThread::GetBlockingPool());
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  driver_.reset(new GCMDriverDesktop(
      gcm_client_factory.Pass(),
      scoped_ptr<IdentityProvider>(new ProfileIdentityProvider(
          SigninManagerFactory::GetForProfile(profile_),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
          login_ui_service)),
      GetChromeBuildInfo(),
      profile_->GetPath().Append(chrome::kGCMStoreDirname),
      profile_->GetRequestContext(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO),
      blocking_task_runner));
#endif
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
