// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_CONTROLLER_H_
#pragma once

#include <vector>

#include "base/linked_ptr.h"
#include "base/singleton.h"
#include "base/timer.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chromeos/wm_message_listener.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/rect.h"

namespace views {
class Widget;
}

class Browser;
class RenderWidgetHost;

namespace chromeos {

class BrowserListener;

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
  // These are the possible layout modes that this controller can be
  // in.  The layout mode is controlled by the window manager.
  enum LayoutMode {
    // ACTIVE_MODE is the mode where chrome takes up the whole screen
    // and the user interacts with it, and this controller hides the
    // snapshots and stops refreshing them.
    ACTIVE_MODE,

    // OVERVIEW_MODE is the mode where the toplevel windows are hidden
    // and the user interacts with the snapshots.  This is when the
    // snapshot windows are shown and actively updated by this
    // controller.
    OVERVIEW_MODE,
  };

  // This class is a singleton.
  static WmOverviewController* GetInstance();

  // BrowserList::Observer methods
  void OnBrowserAdded(const Browser* browser) {}
  void OnBrowserRemoved(const Browser* browser);
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

  // Start reloading snapshots if in overview mode.
  void UpdateSnapshots();

  // Starts the delay timer, and once the delay is over, configures
  // any unconfigured snapshots one at a time until none are left to
  // be configured.
  void StartDelayTimer();

  LayoutMode layout_mode() const { return layout_mode_; }

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

  // Add browser listeners for all existing browsers, reusing any that
  // were already there.
  void AddAllBrowsers();

  // Called when the thumbnail generator notifies us that the snapshot
  // image changed.  This determines which TabContents the given
  // renderer is attached to, and reloads that snapshot.
  void SnapshotImageChanged(RenderWidgetHost* renderer);

  // This is so we can register for notifications.
  NotificationRegistrar registrar_;

  // This is a vector of listeners that listen to all the browsers.
  typedef std::vector<linked_ptr<BrowserListener> > BrowserListenerVector;
  BrowserListenerVector listeners_;

  // This is the bounds of the monitor we're being displayed on. This
  // is used to adjust the size of snapshots so they'll fit.
  gfx::Rect monitor_bounds_;

  // See description above class for details.
  base::OneShotTimer<WmOverviewController> delay_timer_;

  // The current layout mode.
  LayoutMode layout_mode_;

  // This flag is set whenever there is a pending |AskForSnapshot| to Chrome;
  // This is used to prevent |UpdateSnapshots| from being called multiple times.
  bool updating_snapshots_;

  // These indices are used to track the last updated browser listener and tab
  // content, They are used to implement update tab contents in a round-robin
  // fashion.
  int browser_listener_index_;  // index of the last updated browser listener.
  int tab_contents_index_;      // index of the last updated tab contents.

  DISALLOW_COPY_AND_ASSIGN(WmOverviewController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_CONTROLLER_H_
