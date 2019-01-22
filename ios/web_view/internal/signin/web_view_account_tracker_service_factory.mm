// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_account_tracker_service_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewAccountTrackerServiceFactory::WebViewAccountTrackerServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AccountTrackerService",
          BrowserStateDependencyManager::GetInstance()) {
}

// static
AccountTrackerService* WebViewAccountTrackerServiceFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<AccountTrackerService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewAccountTrackerServiceFactory*
WebViewAccountTrackerServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewAccountTrackerServiceFactory> instance;
  return instance.get();
}

void WebViewAccountTrackerServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountTrackerService::RegisterPrefs(registry);
}

std::unique_ptr<KeyedService>
WebViewAccountTrackerServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  std::unique_ptr<AccountTrackerService> service =
      std::make_unique<AccountTrackerService>();
  service->Initialize(browser_state->GetPrefs(), base::FilePath());
  return service;
}

}  // namespace ios_web_view
