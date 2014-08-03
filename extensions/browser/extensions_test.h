// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class ContentClient;
class ContentBrowserClient;
class TestBrowserContext;
}

namespace extensions {
class TestExtensionsBrowserClient;

// Base class for extensions module unit tests of browser process code. Sets up
// the content module and extensions module client interfaces. Initializes
// services for a browser context.
//
// NOTE: Use this class only in extensions_unittests, not in Chrome unit_tests.
// BrowserContextKeyedServiceFactory singletons persist between tests.
// In Chrome those factories assume any BrowserContext is a Profile and will
// cause crashes if it is not. http://crbug.com/395820
class ExtensionsTest : public testing::Test {
 public:
  ExtensionsTest();
  virtual ~ExtensionsTest();

  content::TestBrowserContext* browser_context() {
    return browser_context_.get();
  }
  TestExtensionsBrowserClient* extensions_browser_client() {
    return extensions_browser_client_.get();
  }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  scoped_ptr<content::ContentClient> content_client_;
  scoped_ptr<content::ContentBrowserClient> content_browser_client_;
  scoped_ptr<content::TestBrowserContext> browser_context_;
  scoped_ptr<TestExtensionsBrowserClient> extensions_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_
