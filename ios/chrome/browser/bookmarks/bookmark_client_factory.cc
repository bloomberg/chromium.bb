// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmark_client_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/bookmarks/bookmark_client_impl.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

namespace {

scoped_ptr<KeyedService> BuildBookmarkClientImpl(web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
#if defined(ENABLE_CONFIGURATION_POLICY)
  return make_scoped_ptr(new BookmarkClientImpl(
      browser_state,
      ios::GetKeyedServiceProvider()->GetManagedBookmarkServiceForBrowserState(
          browser_state)));
#else
  return make_scoped_ptr(new BookmarkClientImpl(browser_state, nullptr));
#endif
}

}  // namespace

// static
BookmarkClientImpl* BookmarkClientFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<BookmarkClientImpl*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BookmarkClientFactory* BookmarkClientFactory::GetInstance() {
  return base::Singleton<BookmarkClientFactory>::get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactoryFunction
BookmarkClientFactory::GetDefaultFactory() {
  return &BuildBookmarkClientImpl;
}

BookmarkClientFactory::BookmarkClientFactory()
    : BrowserStateKeyedServiceFactory(
          "BookmarkClient",
          BrowserStateDependencyManager::GetInstance()) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  DependsOn(ios::GetKeyedServiceProvider()->GetManagedBookmarkServiceFactory());
#endif
}

BookmarkClientFactory::~BookmarkClientFactory() {}

scoped_ptr<KeyedService> BookmarkClientFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildBookmarkClientImpl(context).Pass();
}

web::BrowserState* BookmarkClientFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool BookmarkClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
