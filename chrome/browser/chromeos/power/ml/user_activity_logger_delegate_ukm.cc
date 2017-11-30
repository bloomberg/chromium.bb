// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <vector>

#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate_ukm.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"

namespace chromeos {
namespace power {
namespace ml {

// static
int UserActivityLoggerDelegateUkm::BucketEveryFivePercents(int original_value) {
  DCHECK_GE(original_value, 0);
  DCHECK_LE(original_value, 100);
  return 5 * (original_value / 5);
}

UserActivityLoggerDelegateUkm::UserActivityLoggerDelegateUkm()
    : ukm_recorder_(ukm::UkmRecorder::Get()) {}

UserActivityLoggerDelegateUkm::~UserActivityLoggerDelegateUkm() = default;

void UserActivityLoggerDelegateUkm::UpdateOpenTabsURLs() {
  if (!ukm_recorder_)
    return;

  source_ids_.clear();
  for (Browser* browser : *BrowserList::GetInstance()) {
    const TabStripModel* const tab_strip_model = browser->tab_strip_model();
    DCHECK(tab_strip_model);
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      const content::WebContents* const contents =
          tab_strip_model->GetWebContentsAt(i);
      DCHECK(contents);

      const GURL url = contents->GetLastCommittedURL();
      if (!url.SchemeIsHTTPOrHTTPS())
        continue;

      ukm::SourceId source_id = ukm_recorder_->GetNewSourceID();
      // We should run UpdateSourceURL here so that the url is associated with
      // its source id.
      ukm_recorder_->UpdateSourceURL(source_id, url);
      source_ids_.push_back(source_id);
    }
  }
}

void UserActivityLoggerDelegateUkm::LogActivity(
    const UserActivityEvent& event) {
  if (!ukm_recorder_)
    return;

  ukm::SourceId source_id = ukm_recorder_->GetNewSourceID();
  ukm::builders::UserActivity user_activity(source_id);
  user_activity.SetEventType(event.event().type())
      .SetEventReason(event.event().reason())
      .SetLastActivityTime(
          std::floor(event.features().last_activity_time_sec() / 360))
      .SetLastActivityDay(event.features().last_activity_day())
      .SetRecentTimeActive(event.features().recent_time_active_sec())
      .SetDeviceType(event.features().device_type())
      .SetDeviceMode(event.features().device_mode())
      .SetOnBattery(event.features().on_battery());

  if (event.features().has_last_user_activity_time_sec()) {
    user_activity.SetLastUserActivityTime(
        std::floor(event.features().last_user_activity_time_sec() / 360));
  }
  if (event.features().has_time_since_last_mouse_sec()) {
    user_activity.SetTimeSinceLastMouse(
        event.features().time_since_last_mouse_sec());
  }
  if (event.features().has_time_since_last_key_sec()) {
    user_activity.SetTimeSinceLastKey(
        event.features().time_since_last_key_sec());
  }
  user_activity.SetBatteryPercent(
      BucketEveryFivePercents(std::floor(event.features().battery_percent())));

  user_activity.Record(ukm_recorder_);

  for (const ukm::SourceId& id : source_ids_) {
    ukm::builders::UserActivityId(id).SetActivityId(source_id).Record(
        ukm_recorder_);
  }
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
