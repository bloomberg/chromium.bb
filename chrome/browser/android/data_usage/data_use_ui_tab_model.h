// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_UI_TAB_MODEL_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_UI_TAB_MODEL_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace chrome {

namespace android {

// DataUseUITabModel tracks data use tracking start and end transitions on the
// browser's tabs. It serves as a bridge between the DataUseTabModel, which
// lives on the IO thread, browser navigation events and tab closure events,
// which are generated on the UI thread, and UI elements that appear when data
// use tracking starts and ends in a Tab. DataUseUITabModel forwards navigation
// and tab closure events to the DataUseTabModel, and receives tab tracking
// transitions (start/end) from the DataUseTabModel, which it conveys to UI
// notification logic. DataUseUITabModel is not thread-safe, and should be
// accessed only on the UI thread. DataUseUITabModel is registered as a
// DataUseTabModel::TabDataUseObserver, and gets notified when tracking has
// started and ended on a tab. DataUseUITabModel is not thread safe, and should
// only be accessed only on UI thread.
// TODO(tbansal): DataUseTabModel should notify DataUseUITabModel when a tab
// is removed from the list of tabs.
class DataUseUITabModel : public KeyedService,
                          public DataUseTabModel::TabDataUseObserver {
 public:
  DataUseUITabModel();
  ~DataUseUITabModel() override;

  // Reports a browser navigation to the DataUseTabModel on IO thread. Includes
  // the |page_transition|, |tab_id|, and |gurl| for the navigation. Tabs that
  // are restored when Chromium restarts are not reported.
  void ReportBrowserNavigation(const GURL& gurl,
                               ui::PageTransition page_transition,
                               SessionID::id_type tab_id) const;

  // Reports a tab closure for the tab with |tab_id| to the DataUseTabModel on
  // IO thread. The tab could either have been closed or evicted from the memory
  // by Android.
  void ReportTabClosure(SessionID::id_type tab_id);

  // Reports a custom tab navigation to the DataUseTabModel on the IO thread.
  // Includes the |tab_id|, |url|, and |package_name| for the navigation.
  void ReportCustomTabInitialNavigation(SessionID::id_type tab_id,
                                        const std::string& package_name,
                                        const std::string& url);

  // Returns true if data use tracking has been started for the tab with id
  // |tab_id|. Calling this function resets the state of the tab.
  bool CheckAndResetDataUseTrackingStarted(SessionID::id_type tab_id);

  // Returns true if data use tracking has ended for the tab with id |tab_id|.
  // Calling this function resets the state of the tab.
  bool CheckAndResetDataUseTrackingEnded(SessionID::id_type tab_id);

  // Sets the pointer to DataUseTabModel. |data_use_tab_model| is owned by the
  // caller.
  void SetDataUseTabModel(DataUseTabModel* data_use_tab_model);

  // Returns true if the tab with id |tab_id| is currently tracked, and
  // starting the navigation to |url| with transition type |page_transition|
  // would end tracking of data use. Should only be called before the navigation
  // starts.
  bool WouldDataUseTrackingEnd(const std::string& url,
                               int page_transition,
                               SessionID::id_type tab_id) const;

  // Notifies that user clicked "Continue" when the dialog box with data use
  // warning was shown. Includes the |tab_id| on which the warning was shown.
  // When the user clicks "Continue", additional UI warnings about exiting data
  // use tracking are not shown to the user.
  void UserClickedContinueOnDialogBox(SessionID::id_type tab_id);

  base::WeakPtr<DataUseUITabModel> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(DataUseUITabModelTest, ConvertTransitionType);
  FRIEND_TEST_ALL_PREFIXES(DataUseUITabModelTest, EntranceExitState);
  FRIEND_TEST_ALL_PREFIXES(DataUseUITabModelTest, EntranceExitStateForDialog);
  FRIEND_TEST_ALL_PREFIXES(DataUseUITabModelTest, ReportTabEventsTest);

  // DataUseTrackingEvent indicates the state of a tab.
  enum DataUseTrackingEvent {
    // Indicates that data use tracking has started.
    DATA_USE_TRACKING_STARTED,

    // Indicates that data use tracking has ended.
    DATA_USE_TRACKING_ENDED,

    // Indicates that user clicked "Continue" when the dialog box was shown.
    DATA_USE_CONTINUE_CLICKED,
  };

  typedef base::hash_map<SessionID::id_type, DataUseTrackingEvent> TabEvents;

  // DataUseTabModel::TabDataUseObserver implementation:
  void NotifyTrackingStarting(SessionID::id_type tab_id) override;
  void NotifyTrackingEnding(SessionID::id_type tab_id) override;

  // Creates |event| for tab with id |tab_id| and value |event|, if there is no
  // existing entry for |tab_id|, and returns true. Otherwise, returns false
  // without modifying the entry.
  bool MaybeCreateTabEvent(SessionID::id_type tab_id,
                           DataUseTrackingEvent event);

  // Removes event entry for |tab_id|, if the entry is equal to |event|, and
  // returns true. Otherwise, returns false without modifying the entry.
  bool RemoveTabEvent(SessionID::id_type tab_id, DataUseTrackingEvent event);

  // Converts |page_transition| to DataUseTabModel::TransitionType enum.
  // Returns true if conversion was successful, and updates |transition_type|.
  // Otherwise, returns false, and |transition_type| is not changed.
  // |transition_type| must not be null.
  bool ConvertTransitionType(
      ui::PageTransition page_transition,
      DataUseTabModel::TransitionType* transition_type) const;

  // |tab_events_| stores tracking events of multiple tabs.
  TabEvents tab_events_;

  // |data_use_tab_model_| is notified by |this| about browser navigations
  // and tab closures on UI thread.
  base::WeakPtr<DataUseTabModel> data_use_tab_model_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<DataUseUITabModel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataUseUITabModel);
};

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_UI_TAB_MODEL_H_
