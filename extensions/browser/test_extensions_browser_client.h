// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_

#include "base/compiler_specific.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// A simplified ExtensionsBrowserClient for a single normal browser context and
// an optional incognito browser context associated with it. A test that uses
// this class should call ExtensionsBrowserClient::Set() with its instance.
class TestExtensionsBrowserClient : public ExtensionsBrowserClient {
 public:
  // |context| is required and must not be an incognito context.
  TestExtensionsBrowserClient(content::BrowserContext* main_context);
  virtual ~TestExtensionsBrowserClient();

  // Associates an incognito context with |main_context_|.
  void SetIncognitoContext(content::BrowserContext* incognito_context);

  // ExtensionsBrowserClient overrides:
  virtual bool IsShuttingDown() OVERRIDE;
  virtual bool AreExtensionsDisabled(const base::CommandLine& command_line,
                                     content::BrowserContext* context) OVERRIDE;
  virtual bool IsValidContext(content::BrowserContext* context) OVERRIDE;
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) OVERRIDE;
  virtual bool HasOffTheRecordContext(content::BrowserContext* context)
      OVERRIDE;
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) OVERRIDE;
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) OVERRIDE;
  virtual bool IsGuestSession(content::BrowserContext* context) const OVERRIDE;
  virtual bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const OVERRIDE;
  virtual bool CanExtensionCrossIncognito(
      const extensions::Extension* extension,
      content::BrowserContext* context) const OVERRIDE;
  virtual PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) OVERRIDE;
  virtual void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<ExtensionPrefsObserver*>* observers) const OVERRIDE;
  virtual bool DeferLoadingBackgroundHosts(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool IsBackgroundPageAllowed(content::BrowserContext* context) const
      OVERRIDE;
  virtual scoped_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate()
      OVERRIDE;
  virtual bool DidVersionUpdate(content::BrowserContext* context) OVERRIDE;
  virtual scoped_ptr<AppSorting> CreateAppSorting() OVERRIDE;
  virtual bool IsRunningInForcedAppMode() OVERRIDE;
  virtual ApiActivityMonitor* GetApiActivityMonitor(
      content::BrowserContext* context) OVERRIDE;
  virtual ExtensionSystemProvider* GetExtensionSystemFactory() OVERRIDE;
  virtual void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) const OVERRIDE;

 private:
  content::BrowserContext* main_context_;       // Not owned.
  content::BrowserContext* incognito_context_;  // Not owned, defaults to NULL.

  DISALLOW_COPY_AND_ASSIGN(TestExtensionsBrowserClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_
