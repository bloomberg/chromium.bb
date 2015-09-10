// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/enhanced_bookmarks/bookmark_server_cluster_service_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "components/enhanced_bookmarks/bookmark_server_cluster_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

namespace enhanced_bookmarks {

// static
BookmarkServerClusterServiceFactory*
BookmarkServerClusterServiceFactory::GetInstance() {
  return base::Singleton<BookmarkServerClusterServiceFactory>::get();
}

// static
BookmarkServerClusterService*
BookmarkServerClusterServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<BookmarkServerClusterService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

BookmarkServerClusterServiceFactory::BookmarkServerClusterServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BookmarkServerClusterService",
          BrowserStateDependencyManager::GetInstance()) {
  ios::KeyedServiceProvider* provider = ios::GetKeyedServiceProvider();
  DependsOn(OAuth2TokenServiceFactory::GetInstance());
  DependsOn(ios::SigninManagerFactory::GetInstance());
  DependsOn(EnhancedBookmarkModelFactory::GetInstance());
  DependsOn(provider->GetSyncServiceFactory());
}

BookmarkServerClusterServiceFactory::~BookmarkServerClusterServiceFactory() {
}

scoped_ptr<KeyedService>
BookmarkServerClusterServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(!context->IsOffTheRecord());
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  ios::KeyedServiceProvider* provider = ios::GetKeyedServiceProvider();
  return make_scoped_ptr(new BookmarkServerClusterService(
      GetApplicationContext()->GetApplicationLocale(),
      browser_state->GetRequestContext(),
      OAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      ios::SigninManagerFactory::GetForBrowserState(browser_state),
      EnhancedBookmarkModelFactory::GetForBrowserState(browser_state),
      provider->GetSyncServiceForBrowserState(browser_state),
      browser_state->GetPrefs()));
}

web::BrowserState* BookmarkServerClusterServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace enhanced_bookmarks
