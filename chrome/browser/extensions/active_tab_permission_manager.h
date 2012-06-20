// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_MANAGER_H_
#pragma once

#include <set>
#include <string>

#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class TabContents;

namespace extensions {

class Extension;

// Responsible for granting and revoking tab-specific permissions to extensions
// with the activeTab permission.
class ActiveTabPermissionManager : public content::WebContentsObserver,
                                   public content::NotificationObserver {
 public:
  explicit ActiveTabPermissionManager(TabContents* tab_contents);
  virtual ~ActiveTabPermissionManager();

  // If |extension| has the activeTab permission, grants tab-specific
  // permissions to it until the next page navigation or refresh.
  void GrantIfRequested(const Extension* extension);

  // Like above, except make this find the Extension with |extension_id|.
  void GrantIfRequested(const std::string& extension_id);

  // content::WebContentsObserver implementation.
  //
  // This is public to be called from ActiveTabTest as a way to fake creating
  // frames on this tab. It's a bit fragile but there doesn't seem to be any
  // other way to do it.
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;

 private:
  // More content::WebContentsObserver.
  virtual void WebContentsDestroyed(content::WebContents* web_contents)
      OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Clears any granted tab-specific permissions for all extensions and
  // notifies renderers.
  void ClearActiveURLsAndNotify();

  // Inserts a URL pattern giving access to the entire origin for |url|.
  void InsertActiveURL(const GURL& url);

  // Gets the tab's id.
  int tab_id();

  // Gets the current page id.
  int32 GetPageID();

  // Our owning TabContents.
  TabContents* tab_contents_;

  // The URLPatternSet for frames that are active on the current page. This is
  // cleared on navigation.
  //
  // Note that concerned code probably only cares about the origins of these
  // URLs, but let them deal with that.
  URLPatternSet active_urls_;

  // Extensions with the activeTab permission that have been granted
  // tab-specific permissions until the next navigation/refresh.
  ExtensionSet granted_;

  // Listen to extension unloaded notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ActiveTabPermissionManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_MANAGER_H_
