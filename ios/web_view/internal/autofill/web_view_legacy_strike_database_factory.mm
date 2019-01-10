// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/autofill/web_view_legacy_strike_database_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "components/autofill/core/browser/legacy_strike_database.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
autofill::LegacyStrikeDatabase*
WebViewLegacyStrikeDatabaseFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<autofill::LegacyStrikeDatabase*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewLegacyStrikeDatabaseFactory*
WebViewLegacyStrikeDatabaseFactory::GetInstance() {
  return base::Singleton<WebViewLegacyStrikeDatabaseFactory>::get();
}

WebViewLegacyStrikeDatabaseFactory::WebViewLegacyStrikeDatabaseFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillLegacyStrikeDatabase",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewLegacyStrikeDatabaseFactory::~WebViewLegacyStrikeDatabaseFactory() {}

std::unique_ptr<KeyedService>
WebViewLegacyStrikeDatabaseFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<autofill::LegacyStrikeDatabase>(
      browser_state->GetStatePath().Append(
          FILE_PATH_LITERAL("AutofillStrikeDatabase")));
}

}  // namespace ios_web_view
