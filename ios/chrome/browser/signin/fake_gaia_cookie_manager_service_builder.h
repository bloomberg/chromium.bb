// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_BUILDER_H_
#define IOS_CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_BUILDER_H_

#include <memory>

class KeyedService;

namespace web {
class BrowserState;
}

// Helper functions to be used with KeyedService::SetTestingFactory().
// This is a subset of the helpers available in //chrome. If more helpers are
// needed, they can be copied from fake_gaia_cookie_manager_service_builder.h
// under //chrome.
std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerService(
    web::BrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_BUILDER_H_
