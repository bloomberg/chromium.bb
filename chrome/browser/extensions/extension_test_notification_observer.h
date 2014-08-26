// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_types.h"

namespace content {
class WindowedNotificationObserver;
}

// Test helper class for observing extension-related events.
class ExtensionTestNotificationObserver
    : public content::NotificationObserver,
      public extensions::ExtensionActionAPI::Observer {
 public:
  explicit ExtensionTestNotificationObserver(Browser* browser);
  virtual ~ExtensionTestNotificationObserver();

  // Wait for the number of visible page actions to change to |count|.
  bool WaitForPageActionVisibilityChangeTo(int count);

  // Waits until an extension is installed and loaded. Returns true if an
  // install happened before timeout.
  bool WaitForExtensionInstall();

  // Wait for an extension install error to be raised. Returns true if an
  // error was raised.
  bool WaitForExtensionInstallError();

  // Waits until an extension is loaded and all view have loaded.
  void WaitForExtensionAndViewLoad();

  // Waits until an extension is loaded.
  void WaitForExtensionLoad();

  // Waits for an extension load error. Returns true if the error really
  // happened.
  bool WaitForExtensionLoadError();

  // Wait for the specified extension to crash. Returns true if it really
  // crashed.
  bool WaitForExtensionCrash(const std::string& extension_id);

  // Wait for the crx installer to be done. Returns true if it really is done.
  bool WaitForCrxInstallerDone();

  // Wait for all extension views to load.
  bool WaitForExtensionViewsToLoad();

  // Watch for the given event type from the given source.
  // After calling this method, call Wait() to ensure that RunMessageLoop() is
  // called appropriately and cleanup is performed.
  void Watch(int type, const content::NotificationSource& source);

  // After registering one or more event types with Watch(), call
  // this method to run the message loop and perform cleanup.
  void Wait();

  const std::string& last_loaded_extension_id() {
    return last_loaded_extension_id_;
  }
  void set_last_loaded_extension_id(
      const std::string& last_loaded_extension_id) {
    last_loaded_extension_id_ = last_loaded_extension_id;
  }

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  class NotificationSet;

  Profile* GetProfile();

  void WaitForNotification(int notification_type);

  // Wait for |condition_| to be met. |notification_set| is the set of
  // notifications to wait for and to check |condition| when observing. This
  // can be NULL if we are instead waiting for a different observer method, like
  // OnPageActionsUpdated().
  void WaitForCondition(const base::Callback<bool(void)>& condition,
                        NotificationSet* notification_set);

  // Quits the message loop if |condition_| is met.
  void MaybeQuit();

  // extensions::ExtensionActionAPI::Observer:
  virtual void OnPageActionsUpdated(content::WebContents* contents) OVERRIDE;

  Browser* browser_;
  Profile* profile_;

  content::NotificationRegistrar registrar_;
  scoped_ptr<content::WindowedNotificationObserver> observer_;

  std::string last_loaded_extension_id_;
  int extension_installs_observed_;
  int extension_load_errors_observed_;
  int crx_installers_done_observed_;

  // The condition for which we are waiting. This should be checked in any
  // observing methods that could trigger it.
  base::Callback<bool(void)> condition_;

  // The closure to quit the currently-running message loop.
  base::Closure quit_closure_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_NOTIFICATION_OBSERVER_H_
