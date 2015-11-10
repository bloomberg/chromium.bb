// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_

#include <stdint.h>

#include <string>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/android/data_usage/tab_data_use_entry.h"
#include "components/data_usage/core/data_use.h"
#include "url/gurl.h"

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

    // Navigating to another app.
    TRANSITION_TO_EXTERNAL_APP,

    // Navigation from NavSuggest below omnibox.
    TRANSITION_FROM_NAVSUGGEST,

    // Navigation from the omnibox when typing a URL.
    TRANSITION_OMNIBOX_NAVIGATION,

    // Navigation from a bookmark.
    TRANSITION_BOOKMARK,

    // Navigating from history.
    TRANSITION_HISTORY_ITEM,
  };

  // TabDataUseObserver provides the interface for getting notifications from
  // the DataUseTabModel.
  class TabDataUseObserver {
   public:
    virtual ~TabDataUseObserver() {}

    // Notification callback when tab tracking sessions are started and ended.
    // The callback will be received on the same thread AddObserver was called
    // from.
    virtual void NotifyTrackingStarting(int32_t tab_id) = 0;
    virtual void NotifyTrackingEnding(int32_t tab_id) = 0;
  };

  DataUseTabModel(const ExternalDataUseObserver* data_use_observer,
                  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  ~DataUseTabModel();

  base::WeakPtr<DataUseTabModel> GetWeakPtr();

  // Notifies the DataUseTabModel of navigation events. |tab_id| is the source
  // tab of the generated event, |transition| indicates the type of the UI
  // event/transition,  |url| is the URL in the source tab, |package| indicates
  // the android package name of external application that initiated the event.
  void OnNavigationEvent(int32_t tab_id,
                         TransitionType transition,
                         const GURL& url,
                         const std::string& package);

  // Notifies the DataUseTabModel that tab with |tab_id| is closed. Any active
  // tracking sessions for the tab are terminated, and the tab is marked as
  // closed.
  void OnTabCloseEvent(int32_t tab_id);

  // Gets the label for the |data_use| object. |output_label| must not be null.
  // If a tab tracking session is found that was active at the time of request
  // start of |data_use|, returns true and |output_label| is populated with its
  // label. Otherwise returns false and |output_label| is set to empty string.
  bool GetLabelForDataUse(const data_usage::DataUse& data_use,
                          std::string* output_label) const;

  // Adds or removes observers from the observer list. These functions are
  // thread-safe and can be called from any thread.
  void AddObserver(TabDataUseObserver* observer);
  void RemoveObserver(TabDataUseObserver* observer);

 private:
  friend class DataUseTabModelTest;
  friend class MockTabDataUseEntryTest;
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, SingleTabTracking);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, MultipleTabTracking);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, ObserverStartEndEvents);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, ObserverNotNotifiedAfterRemove);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           MultipleObserverMultipleStartEndEvents);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, TabCloseEvent);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest, TabCloseEventEndsTracking);
  FRIEND_TEST_ALL_PREFIXES(DataUseTabModelTest,
                           CompactTabEntriesWithinMaxLimit);

  typedef base::hash_map<int32_t, TabDataUseEntry> TabEntryMap;

  // Returns the maximum number of tab entries to maintain session information
  // about.
  static size_t GetMaxTabEntriesForTests();

  // Notifies the observers that a data usage tracking session started for
  // |tab_id|.
  void NotifyObserversOfTrackingStarting(int32_t tab_id);

  // Notifies the observers that an active data usage tracking session ended for
  // |tab_id|.
  void NotifyObserversOfTrackingEnding(int32_t tab_id);

  // Initiates a new tracking session with the |label| for tab with id |tab_id|.
  void StartTrackingDataUse(int32_t tab_id, const std::string& label);

  // Ends the current tracking session for tab with id |tab_id|.
  void EndTrackingDataUse(int32_t tab_id);

  // Compacts the tab entry map |active_tabs_| by removing expired tab entries.
  // After removing expired tab entries, if the size of |active_tabs_| exceeds
  // |kMaxTabEntries|, oldest unexpired tab entries will be removed until its
  // size is |kMaxTabEntries|.
  void CompactTabEntries();

  // Contains the ExternalDataUseObserver. The caller must ensure that the
  // |data_use_observer_| outlives this instance.
  const ExternalDataUseObserver* data_use_observer_;

  // List of observers waiting for tracking session start and end notifications.
  const scoped_refptr<base::ObserverListThreadSafe<TabDataUseObserver>>
      observer_list_;

  // Maintains the tracking sessions of multiple tabs.
  TabEntryMap active_tabs_;

  base::ThreadChecker thread_checker_;

  // |weak_factory_| is used for generating weak pointers to be passed to
  // DataUseTabUIManager on the UI thread.
  base::WeakPtrFactory<DataUseTabModel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataUseTabModel);
};

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_H_
