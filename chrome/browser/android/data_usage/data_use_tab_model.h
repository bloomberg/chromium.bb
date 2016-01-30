// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/android/data_usage/tab_data_use_entry.h"
#include "components/sessions/core/session_id.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}

class GURL;

namespace chrome {

namespace android {

class DataUseMatcher;
class ExternalDataUseObserver;

// Models tracking and labeling of data usage within each Tab. Within each tab,
// the model tracks the data use of a sequence of navigations in a "tracking
// session" beginning with an entry event and ending with an exit event.
// Typically, these events are navigations matching a URL pattern, or various
// types of browser-initiated navigations. A single tab may have several
// disjoint "tracking sessions" depending on the sequence of entry and exit
// events that took place.
class DataUseTabModel {
 public:
  // TransitionType enumerates the types of possible browser navigation events
  // and transitions.
  enum TransitionType {
    // Navigation from the omnibox to the SRP.
    TRANSITION_OMNIBOX_SEARCH,

    // Navigation from external apps that use Custom Tabs.
    TRANSITION_CUSTOM_TAB,

    // Navigation by clicking a link in the page.
    TRANSITION_LINK,

    // Navigation by reloading the page or restoring tabs.
    TRANSITION_RELOAD,

    // Navigation from the omnibox when typing a URL.
    TRANSITION_OMNIBOX_NAVIGATION,

    // Navigation from a bookmark.
    TRANSITION_BOOKMARK,

    // Navigating from history.
    TRANSITION_HISTORY_ITEM,
  };

  // TabDataUseObserver provides the interface for getting notifications from
  // the DataUseTabModel. TabDataUseObserver is called back on UI thread.
  class TabDataUseObserver {
   public:
    virtual ~TabDataUseObserver() {}

    // Notification callback when tab tracking sessions are started and ended.
    // The callback will be received on the same thread AddObserver was called
    // from.
    virtual void NotifyTrackingStarting(SessionID::id_type tab_id) = 0;
    virtual void NotifyTrackingEnding(SessionID::id_type tab_id) = 0;
  };

  DataUseTabModel();

  // Initializes |this| on UI thread. |external_data_use_observer| is the weak
  // pointer to ExternalDataUseObserver object that owns |this|.
  void InitOnUIThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const base::WeakPtr<ExternalDataUseObserver>& external_data_use_observer);

  virtual ~DataUseTabModel();

  base::WeakPtr<DataUseTabModel> GetWeakPtr();

  // Notifies the DataUseTabModel of navigation events. |tab_id| is the source
  // tab of the generated event, |transition| indicates the type of the UI
  // event/transition,  |url| is the URL in the source tab, |package| indicates
  // the android package name of external application that initiated the event.
  void OnNavigationEvent(SessionID::id_type tab_id,
                         TransitionType transition,
                         const GURL& url,
                         const std::string& package);

  // Notifies the DataUseTabModel that tab with |tab_id| is closed. Any active
  // tracking sessions for the tab are terminated, and the tab is marked as
  // closed.
  void OnTabCloseEvent(SessionID::id_type tab_id);

  // Notifies the DataUseTabModel that tracking label |label| is removed. Any
  // active tracking sessions with the label are ended, without notifying any of
  // the TabDataUseObserver.
  virtual void OnTrackingLabelRemoved(std::string label);

  // Gets the label for the tab with id |tab_id| at time |timestamp|.
  // |output_label| must not be null. If a tab tracking session is found that
  // was active at |timestamp|, returns true and |output_label| is populated
  // with its label. Otherwise, returns false and |output_label| is set to
  // empty string.
  virtual bool GetLabelForTabAtTime(SessionID::id_type tab_id,
                                    base::TimeTicks timestamp,
                                    std::string* output_label) const;

  // Returns true if the navigation event would end the tracking session for
  // |tab_id|. |transition| is the type of the UI event/transition. |url| is the
  // URL in the tab.
  bool WouldNavigationEventEndTracking(SessionID::id_type tab_id,
                                       TransitionType transition,
                                       const GURL& url) const;

  // Adds observers to the observer list. Must be called on UI thread.
  // |observer| is notified on the UI thread.
  void AddObserver(TabDataUseObserver* observer);
  void RemoveObserver(TabDataUseObserver* observer);

  // Called by ExternalDataUseObserver to register multiple case-insensitive
  // regular expressions.
  void RegisterURLRegexes(const std::vector<std::string>& app_package_name,
                          const std::vector<std::string>& domain_path_regex,
                          const std::vector<std::string>& label);

  // Notifies the DataUseTabModel that the external control app is installed or
  // uninstalled. |is_control_app_installed| is true if app is installed.
  void OnControlAppInstallStateChange(bool is_control_app_installed);

  // Returns the maximum number of tracking sessions to maintain per tab.
  size_t max_sessions_per_tab() const { return max_sessions_per_tab_; }

  // Returns the expiration duration for a closed tab entry and an open tab
  // entry respectively.
  const base::TimeDelta& closed_tab_expiration_duration() const {
    return closed_tab_expiration_duration_;
  }
  const base::TimeDelta& open_tab_expiration_duration() const {
    return open_tab_expiration_duration_;
  }

  // Returns the current time.
  base::TimeTicks NowTicks() const;

  // Returns true if the |tab_id| is a custom tab and started tracking due to
  // package name match.
  bool IsCustomTabPackageMatch(SessionID::id_type tab_id) const;

 protected:
  // Notifies the observers that a data usage tracking session started for
  // |tab_id|. Protected for testing.
  void NotifyObserversOfTrackingStarting(SessionID::id_type tab_id);

  // Notifies the observers that an active data usage tracking session ended for
  // |tab_id|. Protected for testing.
  void NotifyObserversOfTrackingEnding(SessionID::id_type tab_id);

 private:
  friend class DataUseTabModelTest;
  friend class ExternalDataUseObserverTest;
  friend class TabDataUseEntryTest;
  friend class TestDataUseTabModel;
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           CompactTabEntriesWithinMaxLimit);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           ExpiredInactiveTabEntryRemovaltimeHistogram);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           MatchingRuleClearedOnControlAppUninstall);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           MultipleObserverMultipleStartEndEvents);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, ObserverStartEndEvents);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, TabCloseEvent);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, TabCloseEventEndsTracking);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           UnexpiredTabEntryRemovaltimeHistogram);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           MatchingRuleFetchOnControlAppInstall);

  typedef base::hash_map<SessionID::id_type, TabDataUseEntry> TabEntryMap;

  // Gets the current label of a tab, and the new label if a navigation event
  // occurs in the tab. |tab_id| is the source tab of the generated event,
  // |transition| indicates the type of the UI event/transition,  |url| is the
  // URL in the source tab, |package| indicates the android package name of
  // external application that initiated the event. |current_label|, |new_label|
  // and |is_package_match| should not be null, and are set with current and new
  // labels respectively. |current_label| will be set to empty string, if there
  // is no active tracking session. |new_label| will be set to empty string if
  // there would be no active tracking session if the navigation happens.
  // |is_package_match| will be set to true if a tracking session will start due
  // to package name match.
  void GetCurrentAndNewLabelForNavigationEvent(SessionID::id_type tab_id,
                                               TransitionType transition,
                                               const GURL& url,
                                               const std::string& package,
                                               std::string* current_label,
                                               std::string* new_label,
                                               bool* is_package_match) const;

  // Initiates a new tracking session with the |label| for tab with id |tab_id|.
  // |is_custom_tab_package_match| is true if |tab_id| is a custom tab and
  // started tracking due to package name match.
  void StartTrackingDataUse(SessionID::id_type tab_id,
                            const std::string& label,
                            bool is_custom_tab_package_match);

  // Ends the current tracking session for tab with id |tab_id|.
  void EndTrackingDataUse(SessionID::id_type tab_id);

  // Compacts the tab entry map |active_tabs_| by removing expired tab entries.
  // After removing expired tab entries, if the size of |active_tabs_| exceeds
  // |kMaxTabEntries|, oldest unexpired tab entries will be removed until its
  // size is |kMaxTabEntries|.
  void CompactTabEntries();

  // Collection of observers that receive tracking session start and end
  // notifications. Notifications are posted on UI thread.
  base::ObserverList<TabDataUseObserver> observers_;

  // Maintains the tracking sessions of multiple tabs.
  TabEntryMap active_tabs_;

  // Maximum number of tab entries to maintain session information about.
  const size_t max_tab_entries_;

  // Maximum number of tracking sessions to maintain per tab.
  const size_t max_sessions_per_tab_;

  // Expiration duration for a closed tab entry and an open tab entry
  // respectively.
  const base::TimeDelta closed_tab_expiration_duration_;
  const base::TimeDelta open_tab_expiration_duration_;

  // TickClock used for obtaining the current time.
  scoped_ptr<base::TickClock> tick_clock_;

  // Stores the matching patterns.
  scoped_ptr<DataUseMatcher> data_use_matcher_;

  // True if the external control app is installed.
  bool is_control_app_installed_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<DataUseTabModel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataUseTabModel);
};

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_
