// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/common/view_type.h"
#include "services/service_manager/public/cpp/binder_registry.h"

class ExtensionFunctionRegistry;
class PrefService;

namespace base {
class CommandLine;
class FilePath;
class ListValue;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}

namespace net {
class NetLog;
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace update_client {
class UpdateClient;
}

namespace extensions {

class ComponentExtensionResourceManager;
class Extension;
class ExtensionCache;
class ExtensionError;
class ExtensionHostDelegate;
class ExtensionPrefsObserver;
class ExtensionApiFrameIdMap;
class ExtensionApiFrameIdMapHelper;
class ExtensionNavigationUIData;
class ExtensionSystem;
class ExtensionSystemProvider;
class ExtensionWebContentsObserver;
class InfoMap;
class KioskDelegate;
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

#if defined(OS_CHROMEOS)
  // Returns a user id hash from |context| or an empty string if no hash could
  // be extracted.
  virtual std::string GetUserIdHashFromContext(
      content::BrowserContext* context) = 0;
#endif

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
  virtual std::unique_ptr<ExtensionHostDelegate>
  CreateExtensionHostDelegate() = 0;

  // Returns true if the client version has updated since the last run. Called
  // once each time the extensions system is loaded per browser_context. The
  // implementation may wish to use the BrowserContext to record the current
  // version for later comparison.
  virtual bool DidVersionUpdate(content::BrowserContext* context) = 0;

  // Permits an external protocol handler to be launched. See
  // ExternalProtocolHandler::PermitLaunchUrl() in Chrome.
  virtual void PermitExternalProtocolHandler() = 0;

  // Return true if the system is run in forced app mode.
  virtual bool IsRunningInForcedAppMode() = 0;

  // Return true if the user is logged in as a public session.
  virtual bool IsLoggedInAsPublicAccount() = 0;

  // Returns the factory that provides an ExtensionSystem to be returned from
  // ExtensionSystem::Get.
  virtual ExtensionSystemProvider* GetExtensionSystemFactory() = 0;

  // Registers extension functions not belonging to the core extensions APIs.
  virtual void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) const = 0;

  // Registers additional interfaces to expose to a RenderFrame.
  virtual void RegisterExtensionInterfaces(
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
          registry,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) const = 0;

  // Creates a RuntimeAPIDelegate responsible for handling extensions
  // management-related events such as update and installation on behalf of the
  // core runtime API implementation.
  virtual std::unique_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const = 0;

  // Returns the manager of resource bundles used in extensions. Returns NULL if
  // the manager doesn't exist.
  virtual const ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() = 0;

  // Propagate a event to all the renderers in every browser context. The
  // implementation must be safe to call from any thread.
  virtual void BroadcastEventToRenderers(
      events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> args) = 0;

  // Returns the embedder's net::NetLog.
  virtual net::NetLog* GetNetLog() = 0;

  // Gets the single ExtensionCache instance shared across the browser process.
  virtual ExtensionCache* GetExtensionCache() = 0;

  // Indicates whether extension update checks should be allowed.
  virtual bool IsBackgroundUpdateAllowed() = 0;

  // Indicates whether an extension update which specifies its minimum browser
  // version as |min_version| can be installed by the client. Not all extensions
  // embedders share the same versioning model, so interpretation of the string
  // is left up to the embedder.
  virtual bool IsMinBrowserVersionSupported(const std::string& min_version) = 0;

  // Embedders can override this function to handle extension errors.
  virtual void ReportError(content::BrowserContext* context,
                           std::unique_ptr<ExtensionError> error);

  // Returns the ExtensionWebContentsObserver for the given |web_contents|.
  virtual ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) = 0;

  // Cleans up browser-side state associated with a WebView that is being
  // destroyed.
  virtual void CleanUpWebView(content::BrowserContext* browser_context,
                              int embedder_process_id,
                              int view_instance_id) {}

  // Attaches the task manager extension tag to |web_contents|, if needed based
  // on |view_type|, so that its corresponding task shows up in the task
  // manager.
  virtual void AttachExtensionTaskManagerTag(content::WebContents* web_contents,
                                             ViewType view_type) {}

  // Returns a new UpdateClient.
  virtual scoped_refptr<update_client::UpdateClient> CreateUpdateClient(
      content::BrowserContext* context);

  virtual std::unique_ptr<ExtensionApiFrameIdMapHelper>
  CreateExtensionApiFrameIdMapHelper(ExtensionApiFrameIdMap* map);

  virtual std::unique_ptr<content::BluetoothChooser> CreateBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler);

  // Returns true if activity logging is enabled for the given |context|.
  virtual bool IsActivityLoggingEnabled(content::BrowserContext* context);

  virtual ExtensionNavigationUIData* GetExtensionNavigationUIData(
      net::URLRequest* request);

  // Returns a delegate that provides kiosk mode functionality.
  virtual KioskDelegate* GetKioskDelegate() = 0;

  // Whether the browser context is associated with Chrome OS lock screen.
  virtual bool IsLockScreenContext(content::BrowserContext* context) = 0;

  // Returns the locale used by the application.
  virtual std::string GetApplicationLocale() = 0;

  // Returns the single instance of |this|.
  static ExtensionsBrowserClient* Get();

  // Initialize the single instance.
  static void Set(ExtensionsBrowserClient* client);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_
