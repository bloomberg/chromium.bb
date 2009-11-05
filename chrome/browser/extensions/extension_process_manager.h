// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/ref_counted.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/view_types.h"

class Browser;
class BrowsingInstance;
class Extension;
class ExtensionHost;
#if defined(TOOLKIT_VIEWS)
class ExtensionView;
#endif
class GURL;
class Profile;
class RenderProcessHost;
class SiteInstance;

// Manages dynamic state of running Chromium extensions.  There is one instance
// of this class per Profile (including OTR).
class ExtensionProcessManager : public NotificationObserver {
 public:
  explicit ExtensionProcessManager(Profile* profile);
  ~ExtensionProcessManager();

  // Creates a new ExtensionHost with its associated view, grouping it in the
  // appropriate SiteInstance (and therefore process) based on the URL and
  // profile.
  ExtensionHost* CreateView(Extension* extension,
                            const GURL& url,
                            Browser* browser,
                            ViewType::Type view_type);
  ExtensionHost* CreateView(const GURL& url,
                            Browser* browser,
                            ViewType::Type view_type);
  ExtensionHost* CreateToolstrip(Extension* extension,
                                 const GURL& url,
                                 Browser* browser);
  ExtensionHost* CreateToolstrip(const GURL& url, Browser* browser);
  ExtensionHost* CreatePopup(Extension* extension,
                             const GURL& url,
                             Browser* browser);
  ExtensionHost* CreatePopup(const GURL& url, Browser* browser);

  // Creates a new UI-less extension instance.  Like CreateView, but not
  // displayed anywhere.
  ExtensionHost* CreateBackgroundHost(Extension* extension, const GURL& url);

  // Returns the SiteInstance that the given URL belongs to.
  SiteInstance* GetSiteInstanceForURL(const GURL& url);

  // Registers an extension process by |extension_id| and specifying which
  // |process_id| it belongs to.
  void RegisterExtensionProcess(const std::string& extension_id,
                                int process_id);

  // Unregisters an extension process with specified |process_id|.
  void UnregisterExtensionProcess(int process_id);

  // Returns the process that the extension with the given ID is running in.
  RenderProcessHost* GetExtensionProcess(const std::string& extension_id);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  typedef std::set<ExtensionHost*> ExtensionHostSet;
  typedef ExtensionHostSet::const_iterator const_iterator;
  const_iterator begin() const { return all_hosts_.begin(); }
  const_iterator end() const { return all_hosts_.end(); }

 private:
  // Called just after |host| is created so it can be registered in our lists.
  void OnExtensionHostCreated(ExtensionHost* host, bool is_background);

  // Called on browser shutdown to close our extension hosts.
  void CloseBackgroundHosts();

  NotificationRegistrar registrar_;

  // The set of all ExtensionHosts managed by this process manager.
  ExtensionHostSet all_hosts_;

  // The set of running viewless background extensions.
  ExtensionHostSet background_hosts_;

  // The BrowsingInstance shared by all extensions in this profile.  This
  // controls process grouping.
  scoped_refptr<BrowsingInstance> browsing_instance_;

  // A map of extension ID to the render_process_id that the extension lives in.
  typedef std::map<std::string, int> ProcessIDMap;
  ProcessIDMap process_ids_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
