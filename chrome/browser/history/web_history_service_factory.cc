// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/web_history_service_factory.h"

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
// Returns true if the user is signed in and full history sync is enabled,
// and false otherwise.
bool IsHistorySyncEnabled(Profile* profile) {
  browser_sync::ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  return sync && sync->IsSyncActive() && !sync->IsLocalSyncEnabled() &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

}  // namespace

// static
WebHistoryServiceFactory* WebHistoryServiceFactory::GetInstance() {
  return base::Singleton<WebHistoryServiceFactory>::get();
}

// static
history::WebHistoryService* WebHistoryServiceFactory::GetForProfile(
      Profile* profile) {
  if (!IsHistorySyncEnabled(profile))
    return nullptr;

  return static_cast<history::WebHistoryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* WebHistoryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  // Ensure that the service is not instantiated or used if the user is not
  // signed into sync, or if web history is not enabled.
  if (!IsHistorySyncEnabled(profile))
    return nullptr;

  return new history::WebHistoryService(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      profile->GetRequestContext());
}

WebHistoryServiceFactory::WebHistoryServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "WebHistoryServiceFactory",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

WebHistoryServiceFactory::~WebHistoryServiceFactory() {
}
