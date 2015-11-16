// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_MESSAGE_FILTER_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_MESSAGE_FILTER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/public/browser/browser_message_filter.h"

namespace startup_metric_utils {

class StartupMetricMessageFilter : public content::BrowserMessageFilter {
 public:
  StartupMetricMessageFilter();
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~StartupMetricMessageFilter() override = default;
  void OnRecordRendererMainEntryTime(
      const base::TimeTicks& renderer_main_entry_time);

  DISALLOW_COPY_AND_ASSIGN(StartupMetricMessageFilter);
};

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_MESSAGE_FILTER_H_
