// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/browser_state.h"

// static
IOSChromeContentSuggestionsServiceFactory*
IOSChromeContentSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeContentSuggestionsServiceFactory>::get();
}

// static
ntp_snippets::ContentSuggestionsService*
IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  return static_cast<ntp_snippets::ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeContentSuggestionsServiceFactory::
    IOSChromeContentSuggestionsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ContentSuggestionsService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeContentSuggestionsServiceFactory::
    ~IOSChromeContentSuggestionsServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeContentSuggestionsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  return base::WrapUnique(new ntp_snippets::ContentSuggestionsService(
      ntp_snippets::ContentSuggestionsService::State::DISABLED));
}
