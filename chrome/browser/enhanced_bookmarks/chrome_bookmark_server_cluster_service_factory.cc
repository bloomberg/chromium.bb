// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enhanced_bookmarks/chrome_bookmark_server_cluster_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enhanced_bookmarks/chrome_bookmark_server_cluster_service.h"
#include "chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/signin_manager.h"

namespace enhanced_bookmarks {

ChromeBookmarkServerClusterServiceFactory::
    ChromeBookmarkServerClusterServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeBookmarkServerClusterService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
}

// static
ChromeBookmarkServerClusterServiceFactory*
ChromeBookmarkServerClusterServiceFactory::GetInstance() {
  return Singleton<ChromeBookmarkServerClusterServiceFactory>::get();
}

// static
ChromeBookmarkServerClusterService*
ChromeBookmarkServerClusterServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK(!context->IsOffTheRecord());
  return static_cast<ChromeBookmarkServerClusterService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService*
ChromeBookmarkServerClusterServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK(!browser_context->IsOffTheRecord());
  Profile* profile = Profile::FromBrowserContext(browser_context);

  return new ChromeBookmarkServerClusterService(
      g_browser_process->GetApplicationLocale(),
      browser_context->GetRequestContext(),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      EnhancedBookmarkModelFactory::GetForBrowserContext(profile),
      profile->GetPrefs(),
      ProfileSyncServiceFactory::GetForProfile(profile));
}

content::BrowserContext*
ChromeBookmarkServerClusterServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace enhanced_bookmarks
