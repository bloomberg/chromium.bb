// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_capability.h"

#include "base/logging.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/signin_error_controller_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"

bool CanPresentAutoLogin(ios::ChromeBrowserState* browser_state) {
  // User must be signed in and not have an error.
  SigninManager* signin =
      ios::SigninManagerFactory::GetForBrowserState(browser_state);
  DCHECK(signin);
  std::string username = signin->GetAuthenticatedAccountInfo().email;
  if (!username.length())
    return false;
  SigninErrorController* error_controller =
      ios::SigninErrorControllerFactory::GetForBrowserState(browser_state);
  DCHECK(error_controller);
  return !error_controller->HasError();
}
