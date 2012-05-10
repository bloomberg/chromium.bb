// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "content/public/common/view_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class Extension;
class ExtensionHost;
class GURL;
class Profile;

namespace content {
class RenderViewHost;
class SiteInstance;
};

// Manages dynamic state of running Chromium extensions. There is one instance
// of this class per Profile. OTR Profiles have a separate instance that keeps
// track of split-mode extensions only.
class ExtensionProcessManager : public content::NotificationObserver {
 public:
  typedef std::set<ExtensionHost*> ExtensionHostSet;
  typedef ExtensionHostSet::const_iterator const_iterator;

  static ExtensionProcessManager* Create(Profile* profile);
  virtual ~ExtensionProcessManager();

  const ExtensionHostSet& background_hosts() const {
    return background_hosts_;
  }

  typedef std::set<content::RenderViewHost*> ViewSet;
  const ViewSet GetAllViews() const;

  // Creates a new ExtensionHost with its associated view, grouping it in the
  // appropriate SiteInstance (and therefore process) based on the URL and
  // profile.
  virtual ExtensionHost* CreateViewHost(const Extension* extension,
                                        const GURL& url,
                                        Browser* browser,
                                        content::ViewType view_type);
  ExtensionHost* CreateViewHost(const GURL& url,
                                Browser* browser,
                                content::ViewType view_type);
  ExtensionHost* CreatePopupHost(const Extension* extension,
                                 const GURL& url,
                                 Browser* browser);
  ExtensionHost* CreatePopupHost(const GURL& url, Browser* browser);
  ExtensionHost* CreateDialogHost(const GURL& url, Browser* browser);
  ExtensionHost* CreateInfobarHost(const Extension* extension,
                                   const GURL& url,
                                   Browser* browser);
  ExtensionHost* CreateInfobarHost(const GURL& url,
                                   Browser* browser);

  // Open the extension's options page.
  void OpenOptionsPage(const Extension* extension, Browser* browser);

  // Creates a new UI-less extension instance.  Like CreateViewHost, but not
  // displayed anywhere.
  virtual void CreateBackgroundHost(const Extension* extension,
                                    const GURL& url);

  // Gets the ExtensionHost for the background page for an extension, or NULL if
  // the extension isn't running or doesn't have a background page.
  ExtensionHost* GetBackgroundHostForExtension(const std::string& extension_id);

  // Returns the SiteInstance that the given URL belongs to.
  // TODO(aa): This only returns correct results for extensions and packaged
  // apps, not hosted apps.
  virtual content::SiteInstance* GetSiteInstanceForURL(const GURL& url);

  // Registers a RenderViewHost as hosting a given extension.
  void RegisterRenderViewHost(content::RenderViewHost* render_view_host,
                              const Extension* extension);

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
  int IncrementLazyKeepaliveCount(const Extension* extension);
  int DecrementLazyKeepaliveCount(const Extension* extension);

  void IncrementLazyKeepaliveCountForView(
      content::RenderViewHost* render_view_host);

  // Handles a response to the ShouldUnload message, used for lazy background
  // pages.
  void OnShouldUnloadAck(const std::string& extension_id, int sequence_id);

  // Same as above, for the Unload message.
  void OnUnloadAck(const std::string& extension_id);

  // Tracks network requests for a given RenderViewHost, used to know
  // when network activity is idle for lazy background pages.
  void OnNetworkRequestStarted(content::RenderViewHost* render_view_host);
  void OnNetworkRequestDone(content::RenderViewHost* render_view_host);

 protected:
  explicit ExtensionProcessManager(Profile* profile);

  // Called just after |host| is created so it can be registered in our lists.
  void OnExtensionHostCreated(ExtensionHost* host, bool is_background);

  // Called on browser shutdown to close our extension hosts.
  void CloseBackgroundHosts();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Gets the profile associated with site_instance_ and all other
  // related SiteInstances.
  Profile* GetProfile() const;

  content::NotificationRegistrar registrar_;

  // The set of ExtensionHosts running viewless background extensions.
  ExtensionHostSet background_hosts_;

  // A SiteInstance related to the SiteInstance for all extensions in
  // this profile.  We create it in such a way that a new
  // browsing instance is created.  This controls process grouping.
  scoped_refptr<content::SiteInstance> site_instance_;

 private:
  // Extra information we keep for each extension's background page.
  struct BackgroundPageData;
  typedef std::string ExtensionId;
  typedef std::map<ExtensionId, BackgroundPageData> BackgroundPageDataMap;

  // Contains all extension-related RenderViewHost instances for all extensions.
  // We also keep a cache of the host's view type, because that information
  // is not accessible at registration/deregistration time.
  typedef std::map<content::RenderViewHost*,
      content::ViewType> ExtensionRenderViews;
  ExtensionRenderViews all_extension_views_;

  // Close the given |host| iff it's a background page.
  void CloseBackgroundHost(ExtensionHost* host);

  // Ensure browser object is not null except for certain situations.
  void EnsureBrowserWhenRequired(Browser* browser,
                                 content::ViewType view_type);

  // These are called when the extension transitions between idle and active.
  // They control the process of closing the background page when idle.
  void OnLazyBackgroundPageIdle(const std::string& extension_id,
                                int sequence_id);
  void OnLazyBackgroundPageActive(const std::string& extension_id);
  void CloseLazyBackgroundPageNow(const std::string& extension_id);

  // Updates a potentially-registered RenderViewHost once it has been
  // associated with a WebContents. This allows us to gather information that
  // was not available when the host was first registered.
  void UpdateRegisteredRenderView(content::RenderViewHost* render_view_host);

  // Clears background page data for this extension.
  void ClearBackgroundPageData(const std::string& extension_id);

  BackgroundPageDataMap background_page_data_;

  // The time to delay between an extension becoming idle and
  // sending a ShouldUnload message; read from command-line switch.
  base::TimeDelta event_page_idle_time_;

  // The time to delay between sending a ShouldUnload message and
  // sending a Unload message; read from command-line switch.
  base::TimeDelta event_page_unloading_time_;

  base::WeakPtrFactory<ExtensionProcessManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
