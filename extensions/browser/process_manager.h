// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_MANAGER_H_
#define EXTENSIONS_BROWSER_PROCESS_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/view_type.h"

class GURL;

namespace content {
class BrowserContext;
class DevToolsAgentHost;
class RenderViewHost;
class RenderFrameHost;
class SiteInstance;
};

namespace extensions {

class Extension;
class ExtensionHost;
class ExtensionRegistry;
class ProcessManagerDelegate;
class ProcessManagerObserver;

// Manages dynamic state of running Chromium extensions. There is one instance
// of this class per Profile. OTR Profiles have a separate instance that keeps
// track of split-mode extensions only.
class ProcessManager : public content::NotificationObserver {
 public:
  typedef std::set<extensions::ExtensionHost*> ExtensionHostSet;
  typedef ExtensionHostSet::const_iterator const_iterator;

  static ProcessManager* Create(content::BrowserContext* context);
  virtual ~ProcessManager();

  const ExtensionHostSet& background_hosts() const {
    return background_hosts_;
  }

  typedef std::set<content::RenderViewHost*> ViewSet;
  const ViewSet GetAllViews() const;

  // The typical observer interface.
  void AddObserver(ProcessManagerObserver* observer);
  void RemoveObserver(ProcessManagerObserver* observer);

  // Creates a new UI-less extension instance.  Like CreateViewHost, but not
  // displayed anywhere.  Returns false if no background host can be created,
  // for example for hosted apps and extensions that aren't enabled in
  // Incognito.
  virtual bool CreateBackgroundHost(const Extension* extension,
                                    const GURL& url);

  // Gets the ExtensionHost for the background page for an extension, or NULL if
  // the extension isn't running or doesn't have a background page.
  ExtensionHost* GetBackgroundHostForExtension(const std::string& extension_id);

  // Returns the SiteInstance that the given URL belongs to.
  // TODO(aa): This only returns correct results for extensions and packaged
  // apps, not hosted apps.
  virtual content::SiteInstance* GetSiteInstanceForURL(const GURL& url);

  // Unregisters a RenderViewHost as hosting any extension.
  void UnregisterRenderViewHost(content::RenderViewHost* render_view_host);

  // Returns all RenderViewHosts that are registered for the specified
  // extension.
  std::set<content::RenderViewHost*> GetRenderViewHostsForExtension(
      const std::string& extension_id);

  // Returns the extension associated with the specified RenderViewHost, or
  // NULL.
  const Extension* GetExtensionForRenderViewHost(
      content::RenderViewHost* render_view_host);

  // Returns true if the (lazy) background host for the given extension has
  // already been sent the unload event and is shutting down.
  bool IsBackgroundHostClosing(const std::string& extension_id);

  // Getter and setter for the lazy background page's keepalive count. This is
  // the count of how many outstanding "things" are keeping the page alive.
  // When this reaches 0, we will begin the process of shutting down the page.
  // "Things" include pending events, resource loads, and API calls.
  int GetLazyKeepaliveCount(const Extension* extension);
  void IncrementLazyKeepaliveCount(const Extension* extension);
  void DecrementLazyKeepaliveCount(const Extension* extension);

  void IncrementLazyKeepaliveCountForView(
      content::RenderViewHost* render_view_host);

  // Keeps a background page alive. Unlike IncrementLazyKeepaliveCount, these
  // impulses will only keep the page alive for a limited amount of time unless
  // called regularly.
  void KeepaliveImpulse(const Extension* extension);

  // Triggers a keepalive impulse for a plug-in (e.g NaCl).
  static void OnKeepaliveFromPlugin(int render_process_id,
                                    int render_frame_id,
                                    const std::string& extension_id);

  // Handles a response to the ShouldSuspend message, used for lazy background
  // pages.
  void OnShouldSuspendAck(const std::string& extension_id, uint64 sequence_id);

  // Same as above, for the Suspend message.
  void OnSuspendAck(const std::string& extension_id);

  // Tracks network requests for a given RenderFrameHost, used to know
  // when network activity is idle for lazy background pages.
  void OnNetworkRequestStarted(content::RenderFrameHost* render_frame_host);
  void OnNetworkRequestDone(content::RenderFrameHost* render_frame_host);

  // Prevents |extension|'s background page from being closed and sends the
  // onSuspendCanceled() event to it.
  void CancelSuspend(const Extension* extension);

  // Creates background hosts if the embedder is ready and they are not already
  // loaded.
  void MaybeCreateStartupBackgroundHosts();

  // Called on shutdown to close our extension hosts.
  void CloseBackgroundHosts();

  // Gets the BrowserContext associated with site_instance_ and all other
  // related SiteInstances.
  content::BrowserContext* GetBrowserContext() const;

  // Sets callbacks for testing keepalive impulse behavior.
  typedef base::Callback<void(const std::string& extension_id)>
      ImpulseCallbackForTesting;
  void SetKeepaliveImpulseCallbackForTesting(
      const ImpulseCallbackForTesting& callback);
  void SetKeepaliveImpulseDecrementCallbackForTesting(
      const ImpulseCallbackForTesting& callback);

  // Sets the time in milliseconds that an extension event page can
  // be idle before it is shut down; must be > 0.
  static void SetEventPageIdleTimeForTesting(unsigned idle_time_msec);

  // Sets the time in milliseconds that an extension event page has
  // between being notified of its impending unload and that unload
  // happening.
  static void SetEventPageSuspendingTimeForTesting(
      unsigned suspending_time_msec);

  // Creates a non-incognito instance for tests. |registry| allows unit tests
  // to inject an ExtensionRegistry that is not managed by the usual
  // BrowserContextKeyedServiceFactory system.
  static ProcessManager* CreateForTesting(content::BrowserContext* context,
                                          ExtensionRegistry* registry);

  // Creates an incognito-context instance for tests.
  static ProcessManager* CreateIncognitoForTesting(
      content::BrowserContext* incognito_context,
      content::BrowserContext* original_context,
      ProcessManager* original_manager,
      ExtensionRegistry* registry);

  bool startup_background_hosts_created_for_test() const {
    return startup_background_hosts_created_;
  }

 protected:
  // If |context| is incognito pass the master context as |original_context|.
  // Otherwise pass the same context for both. Pass the ExtensionRegistry for
  // |context| as |registry|, or override it for testing.
  ProcessManager(content::BrowserContext* context,
                 content::BrowserContext* original_context,
                 ExtensionRegistry* registry);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  // The set of ExtensionHosts running viewless background extensions.
  ExtensionHostSet background_hosts_;

  // A SiteInstance related to the SiteInstance for all extensions in
  // this profile.  We create it in such a way that a new
  // browsing instance is created.  This controls process grouping.
  scoped_refptr<content::SiteInstance> site_instance_;

  // Not owned. Also used by IncognitoProcessManager.
  ExtensionRegistry* extension_registry_;

 private:
  friend class ProcessManagerTest;

  // Extra information we keep for each extension's background page.
  struct BackgroundPageData;
  typedef std::string ExtensionId;
  typedef std::map<ExtensionId, BackgroundPageData> BackgroundPageDataMap;
  typedef std::map<content::RenderViewHost*,
      extensions::ViewType> ExtensionRenderViews;

  // Load all background pages once the profile data is ready and the pages
  // should be loaded.
  void CreateStartupBackgroundHosts();

  // Called just after |host| is created so it can be registered in our lists.
  void OnBackgroundHostCreated(ExtensionHost* host);

  // Close the given |host| iff it's a background page.
  void CloseBackgroundHost(ExtensionHost* host);

  // Internal implementation of DecrementLazyKeepaliveCount with an
  // |extension_id| known to have a lazy background page.
  void DecrementLazyKeepaliveCount(const std::string& extension_id);

  // Checks if keepalive impulses have occured, and adjusts keep alive count.
  void OnKeepaliveImpulseCheck();

  // These are called when the extension transitions between idle and active.
  // They control the process of closing the background page when idle.
  void OnLazyBackgroundPageIdle(const std::string& extension_id,
                                uint64 sequence_id);
  void OnLazyBackgroundPageActive(const std::string& extension_id);
  void CloseLazyBackgroundPageNow(const std::string& extension_id,
                                  uint64 sequence_id);

  // Potentially registers a RenderViewHost, if it is associated with an
  // extension. Does nothing if this is not an extension renderer.
  // Returns true, if render_view_host was registered (it is associated
  // with an extension).
  bool RegisterRenderViewHost(content::RenderViewHost* render_view_host);

  // Unregister RenderViewHosts and clear background page data for an extension
  // which has been unloaded.
  void UnregisterExtension(const std::string& extension_id);

  // Clears background page data for this extension.
  void ClearBackgroundPageData(const std::string& extension_id);

  void OnDevToolsStateChanged(content::DevToolsAgentHost*, bool attached);

  // Contains all active extension-related RenderViewHost instances for all
  // extensions. We also keep a cache of the host's view type, because that
  // information is not accessible at registration/deregistration time.
  ExtensionRenderViews all_extension_views_;

  BackgroundPageDataMap background_page_data_;

  // True if we have created the startup set of background hosts.
  bool startup_background_hosts_created_;

  base::Callback<void(content::DevToolsAgentHost*, bool)> devtools_callback_;

  ImpulseCallbackForTesting keepalive_impulse_callback_for_testing_;
  ImpulseCallbackForTesting keepalive_impulse_decrement_callback_for_testing_;

  ObserverList<ProcessManagerObserver> observer_list_;

  // ID Counter used to set ProcessManager::BackgroundPageData close_sequence_id
  // members. These IDs are tracked per extension in background_page_data_ and
  // are used to verify that nothing has interrupted the process of closing a
  // lazy background process.
  //
  // Any interruption obtains a new ID by incrementing
  // last_background_close_sequence_id_ and storing it in background_page_data_
  // for a particular extension. Callbacks and round-trip IPC messages store the
  // value of the extension's close_sequence_id at the beginning of the process.
  // Thus comparisons can be done to halt when IDs no longer match.
  //
  // This counter provides unique IDs even when BackgroundPageData objects are
  // reset.
  uint64 last_background_close_sequence_id_;

  // Must be last member, see doc on WeakPtrFactory.
  base::WeakPtrFactory<ProcessManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProcessManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_MANAGER_H_
