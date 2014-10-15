// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_CLIENT_H_

#include "base/compiler_specific.h"
#include "extensions/browser/extensions_browser_client.h"

class PrefService;

namespace extensions {

class ExtensionsAPIClient;

// An ExtensionsBrowserClient that supports a single content::BrowserContent
// with no related incognito context.
class ShellExtensionsBrowserClient : public ExtensionsBrowserClient {
 public:
  // |context| is the single BrowserContext used for IsValidContext() below.
  explicit ShellExtensionsBrowserClient(content::BrowserContext* context);
  virtual ~ShellExtensionsBrowserClient();

  // ExtensionsBrowserClient overrides:
  virtual bool IsShuttingDown() override;
  virtual bool AreExtensionsDisabled(const base::CommandLine& command_line,
                                     content::BrowserContext* context) override;
  virtual bool IsValidContext(content::BrowserContext* context) override;
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) override;
  virtual bool HasOffTheRecordContext(
      content::BrowserContext* context) override;
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) override;
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override;
  virtual bool IsGuestSession(content::BrowserContext* context) const override;
  virtual bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const override;
  virtual bool CanExtensionCrossIncognito(
      const Extension* extension,
      content::BrowserContext* context) const override;
  virtual net::URLRequestJob* MaybeCreateResourceBundleRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::FilePath& directory_path,
      const std::string& content_security_policy,
      bool send_cors_header) override;
  virtual bool AllowCrossRendererResourceLoad(
      net::URLRequest* request,
      bool is_incognito,
      const Extension* extension,
      InfoMap* extension_info_map) override;
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
  virtual ExtensionCache* GetExtensionCache() override;
  virtual bool IsBackgroundUpdateAllowed() override;
  virtual bool IsMinBrowserVersionSupported(
      const std::string& min_version) override;

 private:
  // The single BrowserContext for app_shell. Not owned.
  content::BrowserContext* browser_context_;

  // Support for extension APIs.
  scoped_ptr<ExtensionsAPIClient> api_client_;

  // The PrefService for |browser_context_|.
  scoped_ptr<PrefService> prefs_;

  // The extension cache used for download and installation.
  scoped_ptr<ExtensionCache> extension_cache_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsBrowserClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_CLIENT_H_
