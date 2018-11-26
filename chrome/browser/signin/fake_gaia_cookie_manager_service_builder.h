// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_BUILDER_H_
#define CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_BUILDER_H_

#include <memory>

class KeyedService;

namespace content {
class BrowserContext;
}

// Helper functions to be used with KeyedService::SetTestingFactory().
std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerService(
    content::BrowserContext* context);

std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerServiceWithOptions(
    bool create_fake_url_loader_factory_for_cookie_requests,
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_BUILDER_H_
