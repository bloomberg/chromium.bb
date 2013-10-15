// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/custodian_profile_downloader_service_factory.h"

#include "chrome/browser/managed_mode/custodian_profile_downloader_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
CustodianProfileDownloaderService*
CustodianProfileDownloaderServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CustodianProfileDownloaderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CustodianProfileDownloaderServiceFactory*
CustodianProfileDownloaderServiceFactory::GetInstance() {
  return Singleton<CustodianProfileDownloaderServiceFactory>::get();
}

CustodianProfileDownloaderServiceFactory::
CustodianProfileDownloaderServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CustodianProfileDownloaderService",
          BrowserContextDependencyManager::GetInstance()) {
  // Indirect dependency via ProfileDownloader.
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

CustodianProfileDownloaderServiceFactory::
~CustodianProfileDownloaderServiceFactory() {}

BrowserContextKeyedService*
CustodianProfileDownloaderServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new CustodianProfileDownloaderService(static_cast<Profile*>(profile));
}

