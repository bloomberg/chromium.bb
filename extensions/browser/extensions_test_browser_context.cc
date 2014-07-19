// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_test_browser_context.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/test_extensions_browser_client.h"

namespace extensions {

ExtensionsTestBrowserContext::ExtensionsTestBrowserContext() :
  extensions_browser_client_(this) {
  BrowserContextDependencyManager::GetInstance()
      ->CreateBrowserContextServicesForTest(this);
  ExtensionsBrowserClient::Set(&extensions_browser_client_);
}

ExtensionsTestBrowserContext::~ExtensionsTestBrowserContext() {
  BrowserContextDependencyManager::GetInstance()
      ->DestroyBrowserContextServices(this);
  ExtensionsBrowserClient::Set(NULL);
}

}  // namespace extensions
