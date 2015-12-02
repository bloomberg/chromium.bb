// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_

#include <list>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/android/data_usage/tab_data_use_entry.h"
#include "components/sessions/core/session_id.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace data_usage {
struct DataUse;
}

class GURL;

namespace chrome {

namespace android {

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

    // Navigation from external apps such as AGSA app.
    // TODO(rajendrant): Remove this if not needed.
    TRANSITION_FROM_EXTERNAL_APP,

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

  DataUseTabModel(const ExternalDataUseObserver* data_use_observer,
                  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

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
  // active tracking sessions with the label are ended.
  virtual void OnTrackingLabelRemoved(std::string label);

  // Gets the label for the |data_use| object. |output_label| must not be null.
  // If a tab tracking session is found that was active at the time of request
  // start of |data_use|, returns true and |output_label| is populated with its
  // label. Otherwise returns false and |output_label| is set to empty string.
  virtual bool GetLabelForDataUse(const data_usage::DataUse& data_use,
                                  std::string* output_label) const;

  // Adds observers to the observer list. Must be called on IO thread.
  // |observer| is notified on UI thread.
  // TODO(tbansal): Remove observers that have been destroyed.
  void AddObserver(base::WeakPtr<TabDataUseObserver> observer);

 protected:
  // Notifies the observers that a data usage tracking session started for
  // |tab_id|. Protected for testing.
  void NotifyObserversOfTrackingStarting(SessionID::id_type tab_id);

  // Notifies the observers that an active data usage tracking session ended for
  // |tab_id|. Protected for testing.
  void NotifyObserversOfTrackingEnding(SessionID::id_type tab_id);

 private:
  friend class DataUseTabModelTest;
  friend class MockTabDataUseEntryTest;
  friend class TestDataUseTabModel;
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, SingleTabTracking);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, MultipleTabTracking);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, ObserverStartEndEvents);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           MultipleObserverMultipleStartEndEvents);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, TabCloseEvent);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, TabCloseEventEndsTracking);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, OnTrackingLabelRemoved);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           CompactTabEntriesWithinMaxLimit);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           UnexpiredTabEntryRemovaltimeHistogram);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           ExpiredInactiveTabEntryRemovaltimeHistogram);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           ExpiredActiveTabEntryRemovaltimeHistogram);

  typedef base::hash_map<SessionID::id_type, TabDataUseEntry> TabEntryMap;

  // Virtualized for unit test support.
  virtual base::TimeTicks Now() const;

  // Initiates a new tracking session with the |label| for tab with id |tab_id|.
  void StartTrackingDataUse(SessionID::id_type tab_id,
                            const std::string& label);

  // Ends the current tracking session for tab with id |tab_id|.
  void EndTrackingDataUse(SessionID::id_type tab_id);

  // Compacts the tab entry map |active_tabs_| by removing expired tab entries.
  // After removing expired tab entries, if the size of |active_tabs_| exceeds
  // |kMaxTabEntries|, oldest unexpired tab entries will be removed until its
  // size is |kMaxTabEntries|.
  void CompactTabEntries();

  // Contains the ExternalDataUseObserver. The caller must ensure that the
  // |data_use_observer_| outlives this instance.
  const ExternalDataUseObserver* data_use_observer_;

  // Collection of observers that receive tracking session start and end
  // notifications. Notifications are posted on UI thread.
  std::list<base::WeakPtr<TabDataUseObserver>> observers_;

  // Maintains the tracking sessions of multiple tabs.
  TabEntryMap active_tabs_;

  // Maximum number of tab entries to maintain session information about.
  const size_t max_tab_entries_;

  // |ui_task_runner_| is used to notify TabDataUseObserver on UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<DataUseTabModel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataUseTabModel);
};

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_
