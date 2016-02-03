// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_snippets/ios_chrome_ntp_snippets_service_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// static
IOSChromeNTPSnippetsServiceFactory*
IOSChromeNTPSnippetsServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeNTPSnippetsServiceFactory>::get();
}

// static
ntp_snippets::NTPSnippetsService*
IOSChromeNTPSnippetsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  return static_cast<ntp_snippets::NTPSnippetsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeNTPSnippetsServiceFactory::IOSChromeNTPSnippetsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "NTPSnippetsService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeNTPSnippetsServiceFactory::~IOSChromeNTPSnippetsServiceFactory() {}

scoped_ptr<KeyedService>
IOSChromeNTPSnippetsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  DCHECK(!browser_state->IsOffTheRecord());
  return make_scoped_ptr(new ntp_snippets::NTPSnippetsService(
      GetApplicationContext()->GetApplicationLocale()));
}
