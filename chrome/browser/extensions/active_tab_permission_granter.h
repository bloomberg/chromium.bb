// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/tab_capability_tracker.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/url_pattern_set.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

// Responsible for granting and revoking tab-specific permissions to extensions
// with the activeTab or tabCapture permission.
class ActiveTabPermissionGranter : public TabCapabilityTracker::Observer {
 public:
  ActiveTabPermissionGranter(content::WebContents* web_contents,
                             int tab_id,
                             Profile* profile);
  virtual ~ActiveTabPermissionGranter();

  // If |extension| has the activeTab or tabCapture permission, grants
  // tab-specific permissions to it until the next page navigation or refresh.
  void GrantIfRequested(const Extension* extension);

 private:
  // TabCapabilityTracker::Observer implementation.
  virtual void OnGranted(const Extension* extension) OVERRIDE;
  virtual void OnRevoked(const ExtensionSet* extensions) OVERRIDE;

  // Gets the current page id.
  int32 GetPageID();

  // The tab ID for this tab.
  int tab_id_;

  content::WebContents* web_contents_;

  TabCapabilityTracker tab_capability_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ActiveTabPermissionGranter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVE_TAB_PERMISSION_GRANTER_H_
