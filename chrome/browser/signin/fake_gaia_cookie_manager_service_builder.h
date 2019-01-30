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

namespace network {
class TestURLLoaderFactory;
}

// Creates a GaiaCookieManagerService using the supplied
// |test_url_loader_factory| and |context|.
std::unique_ptr<KeyedService> BuildGaiaCookieManagerServiceWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context);

// Builds a FakeGaiaCookieManagerService which uses the provided
// |test_url_loader_factory| for cookie-related requests.
//
// TODO(https://crbug.com/907782): Convert all test code to use
// GaiaCookieManagerService directly, passing a TestURLLoaderFactory when
// fakes are needed.
//
// Once that's done, the method below can be deleted, and this file can be
// renamed to something like gaia_cookie_manager_service_test_util.cc
std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerServiceWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_BUILDER_H_
