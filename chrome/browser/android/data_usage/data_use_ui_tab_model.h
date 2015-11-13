// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_UI_TAB_MODEL_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_UI_TAB_MODEL_H_

#include <stdint.h>

#include <string>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
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
// accessed only on the UI thread.
// TODO(tbansal): DataUseTabModel should notify DataUseUITabModel when a tab
// is removed from the list of tabs.
class DataUseUITabModel : public KeyedService {
 public:
  explicit DataUseUITabModel(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~DataUseUITabModel() override;

  // Reports a browser navigation to the DataUseTabModel on IO thread. Includes
  // the |page_transition|, |tab_id|, and |gurl| for the navigation. Tabs that
  // are restored when Chromium restarts are not reported.
  void ReportBrowserNavigation(const GURL& gurl,
                               ui::PageTransition page_transition,
                               int32_t tab_id) const;

  // Reports a tab closure for the tab with |tab_id| to the DataUseTabModel on
  // IO thread. The tab could either have been closed or evicted from the memory
  // by Android.
  void ReportTabClosure(int32_t tab_id);

  // Reports a custom tab navigation to the DataUseTabModel on the IO thread.
  // Includes the |tab_id|, |url|, and |package_name| for the navigation.
  void ReportCustomTabInitialNavigation(int32_t tab_id,
                                        const std::string& url,
                                        const std::string& package_name);

  // Returns true if data use tracking has been started for the tab with id
  // |tab_id|. Calling this function resets the state of the tab.
  bool HasDataUseTrackingStarted(int32_t tab_id);

  // Returns true if data use tracking has ended for the tab with id |tab_id|.
  // Calling this function resets the state of the tab.
  bool HasDataUseTrackingEnded(int32_t tab_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(DataUseUITabModelTest, EntranceExitState);

  // DataUseTrackingEvent indicates the state of a tab.
  enum DataUseTrackingEvent {
    // Indicates that data use tracking has started.
    DATA_USE_TRACKING_STARTED,

    // Indicates that data use tracking has ended.
    DATA_USE_TRACKING_ENDED,
  };

  typedef std::unordered_map<int32_t, DataUseTrackingEvent> TabEvents;

  // DataUseTabModel::Observer implementation:
  // TODO(tbansal): Add override once DataUseTabModel is checked in.
  void OnTrackingStarted(int32_t tab_id);
  void OnTrackingEnded(int32_t tab_id);

  // Creates |event| for tab with id |tab_id| and value |event|, if there is no
  // existing entry for |tab_id|, and returns true. Otherwise, returns false
  // without modifying the entry.
  bool MaybeCreateTabEvent(int32_t tab_id, DataUseTrackingEvent event);

  // Removes event entry for |tab_id|, if the entry is equal to |event|, and
  // returns true. Otherwise, returns false without modifying the entry.
  bool RemoveTabEvent(int32_t tab_id, DataUseTrackingEvent event);

  // |tab_events_| stores tracking events of multiple tabs.
  TabEvents tab_events_;

  // |io_task_runner_| accesses DataUseTabModel members on IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataUseUITabModel);
};

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_UI_TAB_MODEL_H_
