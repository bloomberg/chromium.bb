// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TAB_CLOSEABLE_STATE_WATCHER_H_
#define CHROME_BROWSER_CHROMEOS_TAB_CLOSEABLE_STATE_WATCHER_H_
#pragma once

#include <vector>

#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_closeable_state_watcher.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "content/common/notification_registrar.h"

namespace chromeos {

// This class overrides ::TabCloseableStateWatcher to allow or disallow tabs or
// browsers to be closed based on increase or decrease in number of tabs or
// browsers.  We only do this on Chromeos and only for non-tests.
//
// Normal session:
// 1) A tab, and hence its containing browser, is not closeable if the tab is
// the last NewTabPage in the last normal non-incognito browser and user is not
// signing off.
// 2) Otherwise, if user closes a normal non-incognito browser or the last tab
// in it, the browser stays open, the existing tabs are closed, and a new
// NewTabPage is opened.
// 3) Or, if user closes a normal incognito browser or the last tab in it, the
// browser closes, a new non-incognito normal browser is opened with a
// NewTabPage (which, by rule 1, will not be closeable).
//
// BWSI session (all browsers are incognito):
// Almost the same as in the normal session, but
// 1) A tab, and hence its containing browser, is not closeable if the tab is
// the last NewTabPage in the last browser (again, all browsers are incognito
// browsers).
// 2-3) Otherwise, if user closes a normal incognito browser or the last tab in
// it, the browser stays open, the existing tabs are closed, and a new
// NewTabPage is open.

class TabCloseableStateWatcher : public ::TabCloseableStateWatcher,
                                 public BrowserList::Observer,
                                 public NotificationObserver {
 public:
  TabCloseableStateWatcher();
  virtual ~TabCloseableStateWatcher();

  // TabCloseableStateWatcher implementation:
  virtual bool CanCloseTab(const Browser* browser) const;
  virtual bool CanCloseBrowser(Browser* browser);
  virtual void OnWindowCloseCanceled(Browser* browser);

 private:
  enum BrowserActionType {
    NONE = 0,         // Nothing to do.
    OPEN_WINDOW = 1,  // Opens a regular (i.e. non-incognito) normal browser.
    OPEN_NTP = 2,     // Opens a NewTabPage.
  };

  // BrowserList::Observer implementation:
  virtual void OnBrowserAdded(const Browser* browser);
  virtual void OnBrowserRemoved(const Browser* browser);

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

  // Called by private class TabStripWatcher for TabStripModelObserver
  // notifications.
  // |closing_last_tab| is true if the tab strip is closing the last tab.
  virtual void OnTabStripChanged(const Browser* browser, bool closing_last_tab);

  // Utility functions.

  // Checks the closeable state of tab.  If it has changed, updates it and
  // notifies all normal browsers.
  // |browser_to_check| is the browser whose closeable state the caller is
  // interested in; can be NULL if caller is interested in all browsers, e.g.
  // when a browser is being removed and there's no specific browser to check.
  void CheckAndUpdateState(const Browser* browser_to_check);

  // Sets the new closeable state and fires notification.
  void SetCloseableState(bool closeable);

  // Returns true if closing of |browser| is permitted.
  // |action_type| is the action to take regardless if browser is closeable.
  bool CanCloseBrowserImpl(const Browser* browser,
                           BrowserActionType* action_type) const;

  // Data members.

  // This is true if tab can be closed; it's updated in CheckAndUpdateState and
  // when sign-off notification is received.
  bool can_close_tab_;

  // This is true when sign-off notification is received; it lets us know to
  // allow closing of all tabs and browsers in this situation.
  bool signing_off_;

  // Is in guest session?
  bool guest_session_;

  NotificationRegistrar notification_registrar_;

  // TabStripWatcher is a TabStripModelObserver that funnels all interesting
  // methods to TabCloseableStateWatcher::OnTabStripChanged. TabStripWatcher is
  // needed so we know which browser the TabStripModelObserver method relates
  // to.
  class TabStripWatcher : public TabStripModelObserver {
   public:
    TabStripWatcher(TabCloseableStateWatcher* main_watcher,
                    const Browser* browser);
    virtual ~TabStripWatcher();

    // TabStripModelObserver implementation:
    virtual void TabInsertedAt(TabContentsWrapper* contents, int index,
                               bool foreground);
    virtual void TabClosingAt(TabStripModel* tab_strip_model,
                              TabContentsWrapper* contents,
                              int index);
    virtual void TabDetachedAt(TabContentsWrapper* contents, int index);
    virtual void TabChangedAt(TabContentsWrapper* contents, int index,
                              TabChangeType change_type);

    const Browser* browser() const {
      return browser_;
    }

   private:
    TabCloseableStateWatcher* main_watcher_;
    const Browser* browser_;

    DISALLOW_COPY_AND_ASSIGN(TabStripWatcher);
  };

  friend class TabStripWatcher;

  std::vector<TabStripWatcher*> tabstrip_watchers_;

  DISALLOW_COPY_AND_ASSIGN(TabCloseableStateWatcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_TAB_CLOSEABLE_STATE_WATCHER_H_
