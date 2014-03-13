// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if defined(OS_ANDROID)
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#else
#include "chrome/browser/signin/mutable_profile_oauth2_token_service.h"
#endif

class ProfileOAuth2TokenServiceWrapperImpl
    : public ProfileOAuth2TokenServiceWrapper {
 public:
  explicit ProfileOAuth2TokenServiceWrapperImpl(Profile* profile);
  virtual ~ProfileOAuth2TokenServiceWrapperImpl();

  // ProfileOAuth2TokenServiceWrapper:
  virtual ProfileOAuth2TokenService* GetProfileOAuth2TokenService() OVERRIDE;

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

 private:
  scoped_ptr<ProfileOAuth2TokenService> profile_oauth2_token_service_;
};

ProfileOAuth2TokenServiceWrapperImpl::ProfileOAuth2TokenServiceWrapperImpl(
    Profile* profile) {
  profile_oauth2_token_service_.reset(new ProfileOAuth2TokenServiceFactory::
                                          PlatformSpecificOAuth2TokenService());
  ChromeSigninClient* client =
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile);
  profile_oauth2_token_service_->Initialize(client, profile);
}

ProfileOAuth2TokenServiceWrapperImpl::~ProfileOAuth2TokenServiceWrapperImpl() {}

void ProfileOAuth2TokenServiceWrapperImpl::Shutdown() {
  profile_oauth2_token_service_->Shutdown();
}

ProfileOAuth2TokenService*
ProfileOAuth2TokenServiceWrapperImpl::GetProfileOAuth2TokenService() {
  return profile_oauth2_token_service_.get();
}

ProfileOAuth2TokenServiceFactory::ProfileOAuth2TokenServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ProfileOAuth2TokenService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
}

ProfileOAuth2TokenServiceFactory::~ProfileOAuth2TokenServiceFactory() {
}

// static
ProfileOAuth2TokenService*
ProfileOAuth2TokenServiceFactory::GetForProfile(Profile* profile) {
  ProfileOAuth2TokenServiceWrapper* wrapper =
      static_cast<ProfileOAuth2TokenServiceWrapper*>(
          GetInstance()->GetServiceForBrowserContext(profile, true));
  if (!wrapper)
    return NULL;
  return wrapper->GetProfileOAuth2TokenService();
}

// static
ProfileOAuth2TokenServiceFactory::PlatformSpecificOAuth2TokenService*
ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
    Profile* profile) {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  return static_cast<PlatformSpecificOAuth2TokenService*>(service);
}

// static
ProfileOAuth2TokenServiceFactory*
    ProfileOAuth2TokenServiceFactory::GetInstance() {
  return Singleton<ProfileOAuth2TokenServiceFactory>::get();
}

KeyedService* ProfileOAuth2TokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new ProfileOAuth2TokenServiceWrapperImpl(profile);
}
