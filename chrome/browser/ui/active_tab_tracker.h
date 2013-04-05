// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIVE_TAB_TRACKER_H_
#define CHROME_BROWSER_UI_ACTIVE_TAB_TRACKER_H_

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/native_focus_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class Browser;

namespace content {
class WebContents;
}

// ActiveTabTracker persists the amount of time the user views a page to
// history. Only pages the user views for more than |kTimeBeforeCommitMS|
// milliseconds while the page is active are persisted.
class ActiveTabTracker : public chrome::BrowserListObserver,
                         public TabStripModelObserver,
                         public content::NotificationObserver,
                         public NativeFocusTrackerHost {
 public:
  ActiveTabTracker();
  virtual ~ActiveTabTracker();

  // TODO(sky): remove this when we have NativeFocusTracker for other platforms.
  bool is_valid() const { return native_focus_tracker_.get() != NULL; }

  // TabStripModelObserver:
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                int reason) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

  // BrowserListObserver:
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // NativeFocusTrackerHost:
  virtual void SetBrowser(Browser* browser) OVERRIDE;

 private:
  // Sets the active webcontents.
  void SetWebContents(content::WebContents* web_contents);

  // Sets the idle state.
  void SetIdleState(IdleState idle_state);

  // Starts the query for the idle state. Invokes SetIdleState() when idel state
  // is found.
  void QueryIdleState();

  // Returns the URL |web_contents_| is showing and needs to be tracked. Returns
  // an empty GURL() is no |web_contents_| or the |web_contents_| is not showing
  // a page that needs to be tracked.
  GURL GetURLFromWebContents() const;

  // If necessary commits the active time for the active tab.
  void CommitActiveTime();

  scoped_ptr<NativeFocusTracker> native_focus_tracker_;

  // The active Browser, or NULL if one is not active.
  Browser* browser_;

  // The active WebContents.
  content::WebContents* web_contents_;

  // Current idle state. Only valid if |browser_| is non-null and
  // |weak_ptr_factory_| is empty.
  IdleState idle_state_;

  // Time the |idle_state_| became IDLE_STATE_ACTIVE, or a tab changed.
  base::TimeTicks active_time_;

  // Timer used to query for idle state.
  base::Timer timer_;

  // WeakPtrFactory used when querying for idle state.
  base::WeakPtrFactory<ActiveTabTracker> weak_ptr_factory_;

  // The url of the active tab. Empty indicates not valid.
  GURL url_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ActiveTabTracker);
};

#endif  // CHROME_BROWSER_UI_ACTIVE_TAB_TRACKER_H_
