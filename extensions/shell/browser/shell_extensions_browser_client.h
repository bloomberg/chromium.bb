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
  virtual bool IsShuttingDown() OVERRIDE;
  virtual bool AreExtensionsDisabled(const base::CommandLine& command_line,
                                     content::BrowserContext* context) OVERRIDE;
  virtual bool IsValidContext(content::BrowserContext* context) OVERRIDE;
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) OVERRIDE;
  virtual bool HasOffTheRecordContext(
      content::BrowserContext* context) OVERRIDE;
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) OVERRIDE;
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) OVERRIDE;
  virtual bool IsGuestSession(content::BrowserContext* context) const OVERRIDE;
  virtual bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const OVERRIDE;
  virtual bool CanExtensionCrossIncognito(
      const Extension* extension,
      content::BrowserContext* context) const OVERRIDE;
  virtual bool IsWebViewRequest(net::URLRequest* request) const OVERRIDE;
  virtual net::URLRequestJob* MaybeCreateResourceBundleRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::FilePath& directory_path,
      const std::string& content_security_policy,
      bool send_cors_header) OVERRIDE;
  virtual bool AllowCrossRendererResourceLoad(
      net::URLRequest* request,
      bool is_incognito,
      const Extension* extension,
      InfoMap* extension_info_map) OVERRIDE;
  virtual PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) OVERRIDE;
  virtual void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<ExtensionPrefsObserver*>* observers) const OVERRIDE;
  virtual ProcessManagerDelegate* GetProcessManagerDelegate() const OVERRIDE;
  virtual scoped_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate()
      OVERRIDE;
  virtual bool DidVersionUpdate(content::BrowserContext* context) OVERRIDE;
  virtual void PermitExternalProtocolHandler() OVERRIDE;
  virtual scoped_ptr<AppSorting> CreateAppSorting() OVERRIDE;
  virtual bool IsRunningInForcedAppMode() OVERRIDE;
  virtual ApiActivityMonitor* GetApiActivityMonitor(
      content::BrowserContext* context) OVERRIDE;
  virtual ExtensionSystemProvider* GetExtensionSystemFactory() OVERRIDE;
  virtual void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) const OVERRIDE;
  virtual scoped_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const OVERRIDE;
  virtual ComponentExtensionResourceManager*
      GetComponentExtensionResourceManager() OVERRIDE;
  virtual net::NetLog* GetNetLog() OVERRIDE;

 private:
  // The single BrowserContext for app_shell. Not owned.
  content::BrowserContext* browser_context_;

  // Support for extension APIs.
  scoped_ptr<ExtensionsAPIClient> api_client_;

  // The PrefService for |browser_context_|.
  scoped_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsBrowserClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_CLIENT_H_
