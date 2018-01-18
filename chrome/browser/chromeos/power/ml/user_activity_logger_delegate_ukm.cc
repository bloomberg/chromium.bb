// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <vector>

#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate_ukm.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {
namespace power {
namespace ml {

// static
int UserActivityLoggerDelegateUkm::BucketEveryFivePercents(int original_value) {
  DCHECK_GE(original_value, 0);
  DCHECK_LE(original_value, 100);
  return 5 * (original_value / 5);
}

int UserActivityLoggerDelegateUkm::ExponentiallyBucketTimestamp(
    int timestamp_sec) {
  DCHECK_GE(timestamp_sec, 0);
  if (timestamp_sec < 60)
    return timestamp_sec;

  if (timestamp_sec < 300) {
    return 10 * (timestamp_sec / 10);
  }

  if (timestamp_sec < 600) {
    return 20 * (timestamp_sec / 20);
  }
  return 600;
}

UserActivityLoggerDelegateUkm::UserActivityLoggerDelegateUkm()
    : ukm_recorder_(ukm::UkmRecorder::Get()) {}

UserActivityLoggerDelegateUkm::~UserActivityLoggerDelegateUkm() = default;

void UserActivityLoggerDelegateUkm::UpdateOpenTabsURLs() {
  if (!ukm_recorder_)
    return;

  source_ids_.clear();
  bool topmost_browser_found = false;
  BrowserList* browser_list = BrowserList::GetInstance();
  DCHECK(browser_list);

  // Go through all browsers starting from last active ones.
  for (auto browser_iterator = browser_list->begin_last_active();
       browser_iterator != browser_list->end_last_active();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;

    const bool is_browser_focused = browser->window()->IsActive();
    const bool is_browser_visible =
        browser->window()->GetNativeWindow()->IsVisible();

    bool is_topmost_browser = false;
    if (is_browser_visible && !topmost_browser_found) {
      is_topmost_browser = true;
      topmost_browser_found = true;
    }

    if (browser->profile()->IsOffTheRecord())
      continue;

    const TabStripModel* const tab_strip_model = browser->tab_strip_model();
    DCHECK(tab_strip_model);

    const int active_tab_index = tab_strip_model->active_index();

    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      DCHECK(contents);
      ukm::SourceId source_id =
          ukm::GetSourceIdForWebContentsDocument(contents);
      if (source_id == ukm::kInvalidSourceId)
        continue;

      const TabProperty tab_property = {i == active_tab_index,
                                        is_browser_focused, is_browser_visible,
                                        is_topmost_browser};

      source_ids_.insert(
          std::pair<ukm::SourceId, TabProperty>(source_id, tab_property));
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
      .SetEventLogDuration(
          ExponentiallyBucketTimestamp(event.event().log_duration_sec()))
      .SetLastActivityTime(
          std::floor(event.features().last_activity_time_sec() / 3600))
      .SetLastActivityDay(event.features().last_activity_day())
      .SetRecentTimeActive(event.features().recent_time_active_sec())
      .SetDeviceType(event.features().device_type())
      .SetDeviceMode(event.features().device_mode());

  if (event.features().has_last_user_activity_time_sec()) {
    user_activity.SetLastUserActivityTime(
        std::floor(event.features().last_user_activity_time_sec() / 3600));
  }
  if (event.features().has_time_since_last_mouse_sec()) {
    user_activity.SetTimeSinceLastMouse(
        event.features().time_since_last_mouse_sec());
  }
  if (event.features().has_time_since_last_key_sec()) {
    user_activity.SetTimeSinceLastKey(
        event.features().time_since_last_key_sec());
  }

  if (event.features().has_on_battery()) {
    user_activity.SetOnBattery(event.features().on_battery());
  }

  if (event.features().has_battery_percent()) {
    user_activity.SetBatteryPercent(BucketEveryFivePercents(
        std::floor(event.features().battery_percent())));
  }

  if (event.features().has_device_management()) {
    user_activity.SetDeviceManagement(event.features().device_management());
  }
  user_activity.Record(ukm_recorder_);

  for (const std::pair<ukm::SourceId, TabProperty>& kv : source_ids_) {
    const ukm::SourceId& id = kv.first;
    const TabProperty& tab_property = kv.second;
    ukm::builders::UserActivityId(id)
        .SetActivityId(source_id)
        .SetIsActive(tab_property.is_active)
        .SetIsBrowserFocused(tab_property.is_browser_focused)
        .SetIsBrowserVisible(tab_property.is_browser_visible)
        .SetIsTopmostBrowser(tab_property.is_topmost_browser)
        .Record(ukm_recorder_);
  }
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
