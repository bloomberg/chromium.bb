// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_origins_seen_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/navigation_metrics/origins_seen_service.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// static
navigation_metrics::OriginsSeenService*
IOSChromeOriginsSeenServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<navigation_metrics::OriginsSeenService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeOriginsSeenServiceFactory*
IOSChromeOriginsSeenServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeOriginsSeenServiceFactory>::get();
}

IOSChromeOriginsSeenServiceFactory::IOSChromeOriginsSeenServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "OriginsSeenService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeOriginsSeenServiceFactory::~IOSChromeOriginsSeenServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeOriginsSeenServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return base::WrapUnique(new navigation_metrics::OriginsSeenService());
}

web::BrowserState* IOSChromeOriginsSeenServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
