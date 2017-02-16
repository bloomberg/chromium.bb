// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gcm/gcm_profile_service_factory.h"

#include <memory>

#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/gcm_driver/gcm_client_factory.h"
#endif

namespace gcm {

// static
GCMProfileService* GCMProfileServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
  // GCM is not supported in incognito mode.
  if (profile->IsOffTheRecord())
    return NULL;

  return static_cast<GCMProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GCMProfileServiceFactory* GCMProfileServiceFactory::GetInstance() {
  return base::Singleton<GCMProfileServiceFactory>::get();
}

GCMProfileServiceFactory::GCMProfileServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "GCMProfileService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(LoginUIServiceFactory::GetInstance());
#endif
}

GCMProfileServiceFactory::~GCMProfileServiceFactory() {
}

KeyedService* GCMProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord());

  base::SequencedWorkerPool* worker_pool =
      content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
#if defined(OS_ANDROID)
  return new GCMProfileService(profile->GetPath(), blocking_task_runner);
#else
  return new GCMProfileService(
      profile->GetPrefs(), profile->GetPath(), profile->GetRequestContext(),
      chrome::GetChannel(),
      gcm::GetProductCategoryForSubtypes(profile->GetPrefs()),
      std::unique_ptr<ProfileIdentityProvider>(new ProfileIdentityProvider(
          SigninManagerFactory::GetForProfile(profile),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          LoginUIServiceFactory::GetShowLoginPopupCallbackForProfile(profile))),
      std::unique_ptr<GCMClientFactory>(new GCMClientFactory),
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI),
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO),
      blocking_task_runner);
#endif
}

content::BrowserContext* GCMProfileServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace gcm
