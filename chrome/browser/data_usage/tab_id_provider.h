// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USAGE_TAB_ID_PROVIDER_H_
#define CHROME_BROWSER_DATA_USAGE_TAB_ID_PROVIDER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/global_request_id.h"

namespace base {
class Location;
class TaskRunner;
}

namespace chrome_browser_data_usage {

// Object that can be attached to a URLRequest to provide the tab ID of that
// URLRequest to callbacks that want them. Callbacks are either run immediately
// if the tab ID is already available, or queued to be run later once the tab ID
// becomes available.
class TabIdProvider : public base::SupportsUserData::Data {
 public:
  struct URLRequestTabInfo {
    URLRequestTabInfo()
        : tab_id(-1),
          main_frame_global_request_id(content::GlobalRequestID()) {}

    URLRequestTabInfo(
        int32_t tab_id,
        const content::GlobalRequestID& main_frame_global_request_id)
        : tab_id(tab_id),
          main_frame_global_request_id(main_frame_global_request_id) {}

    // ID of the Tab for which this request happens. See comments of the same
    // field in data_usage::DataUse for when this is field is populated.
    int32_t tab_id;

    // GlobalRequestID of the mainframe request. See comments of the same field
    // in data_usage::DataUse for when this is field is populated.
    content::GlobalRequestID main_frame_global_request_id;
  };

  // Constructs a tab ID provider, posting the |tab_id_getter| task onto
  // |task_runner|.
  TabIdProvider(base::TaskRunner* task_runner,
                const base::Location& from_here,
                base::OnceCallback<int32_t(void)> tab_id_getter);

  ~TabIdProvider() override;

  // Calls |callback| with the tab ID, either immediately if it's already
  // available, or later once it becomes available.
  void ProvideTabId(base::OnceCallback<void(int32_t)> callback);

  base::WeakPtr<TabIdProvider> GetWeakPtr();

  static const void* const kTabIdProviderUserDataKey;

 private:
  class CallbackRunner;

  // Called when the |tab_info| is ready.
  void OnTabIdReady(int32_t tab_info);

  base::ThreadChecker thread_checker_;
  bool is_tab_info_ready_;
  int32_t tab_info_;
  base::WeakPtr<CallbackRunner> weak_callback_runner_;
  base::WeakPtrFactory<TabIdProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabIdProvider);
};

}  // namespace chrome_browser_data_usage

#endif  // CHROME_BROWSER_DATA_USAGE_TAB_ID_PROVIDER_H_
