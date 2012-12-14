// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_HELPER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_HELPER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class AutomationTabHelper;

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
  // load. |web_contents| will always be valid.
  virtual void OnFirstPendingLoad(content::WebContents* web_contents) { }

  // Called when the tab that had one or more pending loads now has no
  // pending loads. |web_contents| will always be valid.
  //
  // This method will always be called if |OnFirstPendingLoad| was called.
  virtual void OnNoMorePendingLoads(content::WebContents* web_contents) { }

  // Called as a result of a tab being snapshotted.
  virtual void OnSnapshotEntirePageACK(
      bool success,
      const std::vector<unsigned char>& png_data,
      const std::string& error_msg) { }

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
    : public content::WebContentsObserver,
      public base::SupportsWeakPtr<AutomationTabHelper>,
      public content::WebContentsUserData<AutomationTabHelper> {
 public:
  virtual ~AutomationTabHelper();

  void AddObserver(TabEventObserver* observer);
  void RemoveObserver(TabEventObserver* observer);

  // Snapshots the entire page without resizing.
  void SnapshotEntirePage();

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  // Dumps a heap profile.
  void HeapProfilerDump(const std::string& reason);
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

  // Returns true if the tab is loading or the tab is scheduled to load
  // immediately. Note that scheduled loads may be canceled.
  bool has_pending_loads() const;

 private:
  explicit AutomationTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AutomationTabHelper>;

  friend class AutomationTabHelperTest;

  void OnSnapshotEntirePageACK(
      bool success,
      const std::vector<unsigned char>& png_data,
      const std::string& error_msg);

  // content::WebContentsObserver implementation.
  virtual void DidStartLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnWillPerformClientRedirect(int64 frame_id, double delay_seconds);
  void OnDidCompleteOrCancelClientRedirect(int64 frame_id);
  void OnTabOrRenderViewDestroyed(content::WebContents* web_contents);

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
