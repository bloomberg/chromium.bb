// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_HELPER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_HELPER_H_
#pragma once

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class TabContents;
class AutomationTabHelper;

namespace IPC {
class Message;
}

// An observer API implemented by classes which are interested in various
// tab events from AutomationTabHelper(s).
class TabEventObserver {
 public:
  // |LOAD_START| and |LOAD_STOP| notifications may occur several times for a
  // sequence of loads that may appear as one complete navigation to a user.
  // For instance, navigating to a non-existent page will cause a load start
  // and stop for the non-existent page; following that, Chrome will schedule
  // a navigation to an error page which causes another load start and stop.
  //
  // A pending load is a load that is currently in progress or one that is
  // scheduled to occur immediately. The only scheduled loads that are
  // tracked are client redirects, such as javascript redirects.
  // TODO(kkania): Handle redirects that are scheduled to occur later, and
  // change this definition of a pending load.
  // TODO(kkania): Track other types of scheduled navigations.

  // Called when the tab that had no pending loads now has a new pending
  // load. |tab_contents| will always be valid.
  virtual void OnFirstPendingLoad(TabContents* tab_contents) { }

  // Called when the tab that had one or more pending loads now has no
  // pending loads. |tab_contents| will always be valid.
  //
  // This method will always be called if |OnFirstPendingLoad| was called.
  virtual void OnNoMorePendingLoads(TabContents* tab_contents) { }

 protected:
  TabEventObserver();
  virtual ~TabEventObserver();

  // On construction, this class does not observe any events. This method
  // sets us up to observe events from the given |AutomationTabHelper|.
  void StartObserving(AutomationTabHelper* tab_helper);

  // Stop observing events from the given |AutomationTabHelper|. This does not
  // need to be called before the helper dies, and it is ok if this object is
  // destructed while it is still observing an |AutomationTabHelper|.
  void StopObserving(AutomationTabHelper* tab_helper);

 private:
  friend class AutomationTabHelperTest;
  typedef std::vector<base::WeakPtr<AutomationTabHelper> > EventSourceVector;

  // Vector of all the event sources we are observing. Tracked so that this
  // class can remove itself from each source at its destruction.
  EventSourceVector event_sources_;

  DISALLOW_COPY_AND_ASSIGN(TabEventObserver);
};

// Per-tab automation support class. Receives automation/testing messages
// from the renderer. Broadcasts tab events to |TabEventObserver|s.
class AutomationTabHelper
    : public TabContentsObserver,
      public base::SupportsWeakPtr<AutomationTabHelper> {
 public:
  explicit AutomationTabHelper(TabContents* tab_contents);
  virtual ~AutomationTabHelper();

  void AddObserver(TabEventObserver* observer);
  void RemoveObserver(TabEventObserver* observer);

  // Returns true if the tab is loading or the tab is scheduled to load
  // immediately. Note that scheduled loads may be canceled.
  bool has_pending_loads() const;

 private:
  friend class AutomationTabHelperTest;

  // TabContentsObserver implementation.
  virtual void DidStartLoading();
  virtual void DidStopLoading();
  virtual void RenderViewGone();
  virtual void TabContentsDestroyed(TabContents* tab_contents);
  virtual bool OnMessageReceived(const IPC::Message& message);

  void OnWillPerformClientRedirect(int64 frame_id, double delay_seconds);
  void OnDidCompleteOrCancelClientRedirect(int64 frame_id);
  void OnTabOrRenderViewDestroyed(TabContents* tab_contents);

  // True if the tab is currently loading. If a navigation is scheduled but not
  // yet loading, this will be false.
  bool is_loading_;

  // Set of all the frames (by frame ID) that are scheduled to perform a client
  // redirect.
  std::set<int64> pending_client_redirects_;

  // List of all the |TabEventObserver|s, which we broadcast events to.
  ObserverList<TabEventObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AutomationTabHelper);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_HELPER_H_
