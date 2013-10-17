// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TAB_CAPABILITY_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_TAB_CAPABILITY_TRACKER_H_

#include <set>
#include <string>

#include "base/observer_list.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace extensions {

class Extension;

// Grants capabilities to extensions for the lifetime of a tab session.
//
// When Grant() is called, OnGranted() is run for each observer if it hasn't
// already been. When the tab is navigated OnRevoked() is called with every
// extension that had been granted for that session.
class TabCapabilityTracker : public content::WebContentsObserver,
                          public content::NotificationObserver {
 public:
  // Observer for tab session changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when Grant() is called and |extension| hasn't already been
    // granted.
    virtual void OnGranted(const Extension* extension) = 0;

    // Called when the tab is navigated or destroyed with every extension that
    // had been Grant()ed.
    virtual void OnRevoked(const ExtensionSet* extensions) = 0;
  };

  TabCapabilityTracker(content::WebContents* web_contents, Profile* profile);
  virtual ~TabCapabilityTracker();

  // Grants the capability to the extension (idempotent).
  void Grant(const Extension* extension);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents* web_contents)
      OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Revokes the capabilities of every extension and calls OnRevoked.
  void ClearActiveExtensionsAndNotify();

  // The extensions that have been granted capabilities so far in this tab
  // session.
  ExtensionSet granted_extensions_;

  ObserverList<Observer> observers_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabCapabilityTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TAB_CAPABILITY_TRACKER_H_
