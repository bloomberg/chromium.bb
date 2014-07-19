// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSIONS_TEST_BROWSER_CONTEXT_H_
#define EXTENSIONS_COMMON_EXTENSIONS_TEST_BROWSER_CONTEXT_H_

#include "content/public/test/test_browser_context.h"
#include "extensions/browser/test_extensions_browser_client.h"

namespace extensions {

// An implementation of BrowserContext used for extension testing. It will set
// necessary extension state like the ExtensionsBrowserClient and any keyed
// services, and cleans up after itself on destruction.
class ExtensionsTestBrowserContext : public content::TestBrowserContext {
 public:
  ExtensionsTestBrowserContext();
  virtual ~ExtensionsTestBrowserContext();

 private:
  TestExtensionsBrowserClient extensions_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsTestBrowserContext);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSIONS_TEST_BROWSER_CONTEXT_H_
