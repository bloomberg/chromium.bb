// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_GAIA_COOKIE_MANAGER_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_SIGNIN_GAIA_COOKIE_MANAGER_SERVICE_TEST_UTIL_H_

#include <memory>

class KeyedService;

namespace content {
class BrowserContext;
}

namespace network {
class TestURLLoaderFactory;
}

// Creates a GaiaCookieManagerService using the supplied
// |test_url_loader_factory| and |context|.
std::unique_ptr<KeyedService> BuildGaiaCookieManagerServiceWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_SIGNIN_GAIA_COOKIE_MANAGER_SERVICE_TEST_UTIL_H_
