// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_error_controller_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"

namespace ios {

// static
SigninErrorController* SigninErrorControllerFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SigninErrorController*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SigninErrorControllerFactory* SigninErrorControllerFactory::GetInstance() {
  return base::Singleton<SigninErrorControllerFactory>::get();
}

SigninErrorControllerFactory::SigninErrorControllerFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninErrorController",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

SigninErrorControllerFactory::~SigninErrorControllerFactory() {
}

std::unique_ptr<KeyedService>
SigninErrorControllerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SigninErrorController>(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(
          chrome_browser_state));
}

}  // namespace ios
