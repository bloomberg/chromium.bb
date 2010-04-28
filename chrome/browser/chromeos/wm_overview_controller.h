// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_CONTROLLER_H_

#include <vector>

#include "base/linked_ptr.h"
#include "base/singleton.h"
#include "base/timer.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chromeos/wm_message_listener.h"
#include "chrome/common/notification_registrar.h"
#include "gfx/rect.h"

namespace views {
class Widget;
}

class Animation;
class Browser;

namespace chromeos {

class BrowserListener;
class WmOverviewSnapshot;

// WmOverviewController is responsible for managing a list of objects
// that listen to the browsers (BrowserListeners, defined in the
// source file for this class) for changes, and keep a list of
// snapshot images in sync with the browser tab contents.
//
// As tabs are added/removed from the browsers, the number of snapshot
// windows changes to match.
//
// As obtaining and setting snapshots is expensive we delay setting
// the snapshot. The delay is controlled by delay_timer_. Once the
// timer fires another timer is started (configure_timer_). This timer
// invokes ConfigureNextUnconfiguredCell on the BrowserListener, which
// obtains and sets the snapshot of the next uncofigured
// cell. ConfigureNextUnconfiguredCell only configures one cell at a
// time until they are all configured.

class WmOverviewController : public BrowserList::Observer,
                             public WmMessageListener::Observer,
                             public NotificationObserver {
 public:
  // This class is a singleton.
  static WmOverviewController* instance();

  // BrowserList::Observer methods
  // This is called immediately after a browser is added to the list.
  void OnBrowserAdded(const Browser* browser) {}

  // This is called immediately before a browser is removed from the list.
  void OnBrowserRemoving(const Browser* browser);
  // End BrowserList::Observer methods

  // WmMessageListener::Observer methods
  // This is called immediately after a browser is added to the list.
  void ProcessWmMessage(const WmIpc::Message& message,
                        GdkWindow* window);
  // End WmMessageListener::Observer methods

  // NotificationObserver methods
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);
  // End NotificationObserver methods

  // Used by the BrowserListeners to configure their snapshots.
  const gfx::Rect& monitor_bounds() const { return monitor_bounds_; }

  // Tells the listeners whether or not they're allowed to show
  // snapshots yet.
  bool allow_show_snapshots() const { return allow_show_snapshots_; }

  // Starts the delay timer, and once the delay is over, configures
  // any unconfigured snapshots one at a time until none are left to
  // be configured.
  void StartDelayTimer();

 private:
  friend struct DefaultSingletonTraits<WmOverviewController>;

  // This class is a singleton.
  WmOverviewController();
  ~WmOverviewController();

  // Restores tab selections on all browsers to what they were when
  // Show was last called.  Used when cancelling overview mode.
  void RestoreTabSelections();

  // Saves the currently selected tabs in the snapshots so that they
  // can be restored later with RestoreTabSelections.
  void SaveTabSelections();

  // Show the snapshot windows, saving current tab selections.
  void Show();

  // Hide the snapshot windows.  When |cancelled| is true, then the
  // tab selections that were saved when the snapshot windows were
  // shown are restored.
  void Hide(bool cancelled);

  // Invoked by delay_timer_. Sets allow_show_snapshots_ to true and starts
  // configure_timer_.
  void StartConfiguring();

  // Configure the next unconfigured snapshot window owned by any of
  // the listeners.
  void ConfigureNextUnconfiguredSnapshot();

  // Add browser listeners for all existing browsers, reusing any that
  // were already there.
  void AddAllBrowsers();

  // This is so we can register for notifications.
  NotificationRegistrar registrar_;

  // This is a vector of listeners that listen to all the browsers.
  typedef std::vector<linked_ptr<BrowserListener> > BrowserListenerVector;
  BrowserListenerVector listeners_;

  // This is the bounds of the monitor we're being displayed on. This
  // is used to adjust the size of snapshots so they'll fit.
  gfx::Rect monitor_bounds_;

  // This indicates whether we should actually set the snapshots so we
  // don't do it when we don't need to. It is initially false, then
  // set to true by StartConfiguring.
  bool allow_show_snapshots_;

  // See description above class for details.
  base::OneShotTimer<WmOverviewController> delay_timer_;

  // See description above class for details.
  base::RepeatingTimer<WmOverviewController> configure_timer_;

  DISALLOW_COPY_AND_ASSIGN(WmOverviewController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_CONTROLLER_H_
