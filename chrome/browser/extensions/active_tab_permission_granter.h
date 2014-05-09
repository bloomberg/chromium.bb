// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_

#include <set>
#include <string>

#include "base/scoped_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/url_pattern_set.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;
class ExtensionRegistry;

// Responsible for granting and revoking tab-specific permissions to extensions
// with the activeTab or tabCapture permission.
class ActiveTabPermissionGranter
    : public content::WebContentsObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  ActiveTabPermissionGranter(content::WebContents* web_contents,
                             int tab_id,
                             Profile* profile);
  virtual ~ActiveTabPermissionGranter();

  // If |extension| has the activeTab or tabCapture permission, grants
  // tab-specific permissions to it until the next page navigation or refresh.
  void GrantIfRequested(const Extension* extension);

 private:
  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // extensions::ExtensionRegistryObserver implementation.
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionInfo::Reason reason)
      OVERRIDE;

  // Clears any tab-specific permissions for all extensions on |tab_id_| and
  // notifies renderers.
  void ClearActiveExtensionsAndNotify();

  // The tab ID for this tab.
  int tab_id_;

  // Extensions with the activeTab permission that have been granted
  // tab-specific permissions until the next navigation/refresh.
  ExtensionSet granted_extensions_;

  // Listen to extension unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ActiveTabPermissionGranter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_
