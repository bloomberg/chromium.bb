// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "extensions/browser/extension_prefs_observer.h"

class ExtensionFunctionRegistry;
class PrefService;

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace net {
class NetLog;
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace extensions {

class ApiActivityMonitor;
class AppSorting;
class ComponentExtensionResourceManager;
class Extension;
class ExtensionHostDelegate;
class ExtensionPrefsObserver;
class ExtensionSystem;
class ExtensionSystemProvider;
class InfoMap;
class ProcessManagerDelegate;
class RuntimeAPIDelegate;

// Interface to allow the extensions module to make browser-process-specific
// queries of the embedder. Should be Set() once in the browser process.
//
// NOTE: Methods that do not require knowledge of browser concepts should be
// added in ExtensionsClient (extensions/common/extensions_client.h) even if
// they are only used in the browser process.
class ExtensionsBrowserClient {
 public:
  virtual ~ExtensionsBrowserClient() {}

  // Returns true if the embedder has started shutting down.
  virtual bool IsShuttingDown() = 0;

  // Returns true if extensions have been disabled (e.g. via a command-line flag
  // or preference).
  virtual bool AreExtensionsDisabled(const base::CommandLine& command_line,
                                     content::BrowserContext* context) = 0;

  // Returns true if the |context| is known to the embedder.
  virtual bool IsValidContext(content::BrowserContext* context) = 0;

  // Returns true if the BrowserContexts could be considered equivalent, for
  // example, if one is an off-the-record context owned by the other.
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) = 0;

  // Returns true if |context| has an off-the-record context associated with it.
  virtual bool HasOffTheRecordContext(content::BrowserContext* context) = 0;

  // Returns the off-the-record context associated with |context|. If |context|
  // is already off-the-record, returns |context|.
  // WARNING: This may create a new off-the-record context. To avoid creating
  // another context, check HasOffTheRecordContext() first.
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) = 0;

  // Returns the original "recording" context. This method returns |context| if
  // |context| is not incognito.
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) = 0;

  // Returns true if |context| corresponds to a guest session.
  virtual bool IsGuestSession(content::BrowserContext* context) const = 0;

  // Returns true if |extension_id| can run in an incognito window.
  virtual bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const = 0;

  // Returns true if |extension| can see events and data from another
  // sub-profile (incognito to original profile, or vice versa).
  virtual bool CanExtensionCrossIncognito(
      const extensions::Extension* extension,
      content::BrowserContext* context) const = 0;

  // Returns true if |request| corresponds to a resource request from a
  // <webview>.
  virtual bool IsWebViewRequest(net::URLRequest* request) const = 0;

  // Returns an URLRequestJob to load an extension resource from the embedder's
  // resource bundle (.pak) files. Returns NULL if the request is not for a
  // resource bundle resource or if the embedder does not support this feature.
  // Used for component extensions. Called on the IO thread.
  virtual net::URLRequestJob* MaybeCreateResourceBundleRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::FilePath& directory_path,
      const std::string& content_security_policy,
      bool send_cors_header) = 0;

  // Returns true if the embedder wants to allow a chrome-extension:// resource
  // request coming from renderer A to access a resource in an extension running
  // in renderer B. For example, Chrome overrides this to provide support for
  // webview and dev tools. Called on the IO thread.
  virtual bool AllowCrossRendererResourceLoad(net::URLRequest* request,
                                              bool is_incognito,
                                              const Extension* extension,
                                              InfoMap* extension_info_map) = 0;

  // Returns the PrefService associated with |context|.
  virtual PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) = 0;

  // Populates a list of ExtensionPrefs observers to be attached to each
  // BrowserContext's ExtensionPrefs upon construction. These observers
  // are not owned by ExtensionPrefs.
  virtual void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<ExtensionPrefsObserver*>* observers) const = 0;

  // Returns the ProcessManagerDelegate shared across all BrowserContexts. May
  // return NULL in tests or for simple embedders.
  virtual ProcessManagerDelegate* GetProcessManagerDelegate() const = 0;

  // Creates a new ExtensionHostDelegate instance.
  virtual scoped_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate() = 0;

  // Returns true if the client version has updated since the last run. Called
  // once each time the extensions system is loaded per browser_context. The
  // implementation may wish to use the BrowserContext to record the current
  // version for later comparison.
  virtual bool DidVersionUpdate(content::BrowserContext* context) = 0;

  // Permits an external protocol handler to be launched. See
  // ExternalProtocolHandler::PermitLaunchUrl() in Chrome.
  virtual void PermitExternalProtocolHandler() = 0;

  // Creates a new AppSorting instance.
  virtual scoped_ptr<AppSorting> CreateAppSorting() = 0;

  // Return true if the system is run in forced app mode.
  virtual bool IsRunningInForcedAppMode() = 0;

  // Returns the embedder's ApiActivityMonitor for |context|. Returns NULL if
  // the embedder does not monitor extension API activity.
  virtual ApiActivityMonitor* GetApiActivityMonitor(
      content::BrowserContext* context) = 0;

  // Returns the factory that provides an ExtensionSystem to be returned from
  // ExtensionSystem::Get.
  virtual ExtensionSystemProvider* GetExtensionSystemFactory() = 0;

  // Registers extension functions not belonging to the core extensions APIs.
  virtual void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) const = 0;

  // Creates a RuntimeAPIDelegate responsible for handling extensions
  // management-related events such as update and installation on behalf of the
  // core runtime API implementation.
  virtual scoped_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const = 0;

  // Returns the manager of resource bundles used in extensions. Returns NULL if
  // the manager doesn't exist.
  virtual ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() = 0;

  // Returns the embedder's net::NetLog.
  virtual net::NetLog* GetNetLog() = 0;

  // Returns the single instance of |this|.
  static ExtensionsBrowserClient* Get();

  // Initialize the single instance.
  static void Set(ExtensionsBrowserClient* client);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_
