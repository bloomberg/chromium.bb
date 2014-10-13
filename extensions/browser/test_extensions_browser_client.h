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
  // |main_context| is required and must not be an incognito context.
  explicit TestExtensionsBrowserClient(content::BrowserContext* main_context);
  virtual ~TestExtensionsBrowserClient();

  void set_process_manager_delegate(ProcessManagerDelegate* delegate) {
    process_manager_delegate_ = delegate;
  }
  void set_extension_system_factory(ExtensionSystemProvider* factory) {
    extension_system_factory_ = factory;
  }

  // Associates an incognito context with |main_context_|.
  void SetIncognitoContext(content::BrowserContext* incognito_context);

  // ExtensionsBrowserClient overrides:
  virtual bool IsShuttingDown() override;
  virtual bool AreExtensionsDisabled(const base::CommandLine& command_line,
                                     content::BrowserContext* context) override;
  virtual bool IsValidContext(content::BrowserContext* context) override;
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) override;
  virtual bool HasOffTheRecordContext(content::BrowserContext* context)
      override;
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) override;
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override;
  virtual bool IsGuestSession(content::BrowserContext* context) const override;
  virtual bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const override;
  virtual bool CanExtensionCrossIncognito(
      const extensions::Extension* extension,
      content::BrowserContext* context) const override;
  virtual net::URLRequestJob* MaybeCreateResourceBundleRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::FilePath& directory_path,
      const std::string& content_security_policy,
      bool send_cors_header) override;
  virtual bool AllowCrossRendererResourceLoad(net::URLRequest* request,
                                              bool is_incognito,
                                              const Extension* extension,
                                              InfoMap* extension_info_map)
      override;
  virtual PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override;
  virtual void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<ExtensionPrefsObserver*>* observers) const override;
  virtual ProcessManagerDelegate* GetProcessManagerDelegate() const override;
  virtual scoped_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate()
      override;
  virtual bool DidVersionUpdate(content::BrowserContext* context) override;
  virtual void PermitExternalProtocolHandler() override;
  virtual scoped_ptr<AppSorting> CreateAppSorting() override;
  virtual bool IsRunningInForcedAppMode() override;
  virtual ApiActivityMonitor* GetApiActivityMonitor(
      content::BrowserContext* context) override;
  virtual ExtensionSystemProvider* GetExtensionSystemFactory() override;
  virtual void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) const override;
  virtual scoped_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const override;
  virtual ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() override;
  virtual void BroadcastEventToRenderers(
      const std::string& event_name,
      scoped_ptr<base::ListValue> args) override;
  virtual net::NetLog* GetNetLog() override;

 private:
  content::BrowserContext* main_context_;       // Not owned.
  content::BrowserContext* incognito_context_;  // Not owned, defaults to NULL.

  // Not owned, defaults to NULL.
  ProcessManagerDelegate* process_manager_delegate_;

  // Not owned, defaults to NULL.
  ExtensionSystemProvider* extension_system_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionsBrowserClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_
