// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/prerender_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/prerender/prerender_service.h"
#include "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
PrerenderService* PrerenderServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<PrerenderService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
PrerenderServiceFactory* PrerenderServiceFactory::GetInstance() {
  static base::NoDestructor<PrerenderServiceFactory> instance;
  return instance.get();
}

PrerenderServiceFactory::PrerenderServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PrerenderService",
          BrowserStateDependencyManager::GetInstance()) {}

PrerenderServiceFactory::~PrerenderServiceFactory() {}

std::unique_ptr<KeyedService> PrerenderServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<PrerenderService>(browser_state);
}

bool PrerenderServiceFactory::ServiceIsNULLWhileTesting() const {
  // PrerenderService is omitted while testing because it complicates
  // measurements in perf tests.
  return true;
}
