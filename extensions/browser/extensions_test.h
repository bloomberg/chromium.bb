// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/mock_extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionPrefValueMap;
class PrefService;

namespace content {
class BrowserContext;
class ContentBrowserClient;
class ContentUtilityClient;
class RenderViewHostTestEnabler;
}

namespace extensions {
class TestExtensionsBrowserClient;

// Base class for extensions module unit tests of browser process code. Sets up
// the content module and extensions module client interfaces. Initializes
// services for a browser context and sets up extension preferences.
//
// NOTE: Use this class only in extensions_unittests, not in Chrome unit_tests.
// In Chrome those factories assume any BrowserContext is a Profile and will
// cause crashes if it is not. http://crbug.com/395820
class ExtensionsTest : public testing::Test {
 public:
  ExtensionsTest();
  // If the test uses a TestBrowserThreadBundle, then it must be given to the
  // constructor here so that this class can use its MessageLoop.
  explicit ExtensionsTest(
      std::unique_ptr<content::TestBrowserThreadBundle> thread_bundle);
  ~ExtensionsTest() override;

  // Allows setting a custom TestExtensionsBrowserClient. Must only be called
  // before SetUp().
  void SetExtensionsBrowserClient(
      std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client);

  // Returned as a BrowserContext since most users don't need methods from
  // TestBrowserContext.
  content::BrowserContext* browser_context() { return browser_context_.get(); }

  // Returns the incognito context associated with the ExtensionsBrowserClient.
  content::BrowserContext* incognito_context() {
    return incognito_context_.get();
  }

  // Returned as a TestExtensionsBrowserClient since most users need to call
  // test-specific methods on it.
  TestExtensionsBrowserClient* extensions_browser_client() {
    return extensions_browser_client_.get();
  }

  PrefService* pref_service() { return pref_service_.get(); }

  // testing::Test overrides:
  void SetUp() override;
  void TearDown() override;

 private:
  content::TestContentClientInitializer content_client_initializer_;
  std::unique_ptr<content::ContentBrowserClient> content_browser_client_;
  std::unique_ptr<content::ContentUtilityClient> content_utility_client_;
  std::unique_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<content::BrowserContext> incognito_context_;
  std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client_;
  std::unique_ptr<ExtensionPrefValueMap> extension_pref_value_map_;
  std::unique_ptr<PrefService> pref_service_;

  MockExtensionSystemFactory<MockExtensionSystem> extension_system_factory_;

  // Optional thread bundle for some subclasses. Needs to exist before
  // the RenderViewHostTestEnabler if it is going to exist.
  std::unique_ptr<content::TestBrowserThreadBundle> thread_bundle_;

  // The existence of this object enables tests via
  // RenderViewHostTester.
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_
