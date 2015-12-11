// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_model.h"

#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/android/data_usage/data_use_matcher.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "components/data_usage/core/data_use.h"
#include "components/variations/variations_associated_data.h"
#include "url/gurl.h"

namespace {

// Default maximum number of tabs to maintain session information about. May be
// overridden by the field trial.
const size_t kDefaultMaxTabEntries = 200;

// Default maximum number of tracking session history to maintain per tab. May
// be overridden by the field trial.
const size_t kDefaultMaxSessionsPerTab = 5;

// Default expiration duration in seconds, after which a closed and an open tab
// entry can be removed from the list of tab entries, respectively. May be
// overridden by the field trial.
const uint32_t kDefaultClosedTabExpirationDurationSeconds = 30;  // 30 seconds.
const uint32_t kDefaultOpenTabExpirationDurationSeconds =
    60 * 60 * 24 * 5;  // 5 days.

const char kUMAExpiredInactiveTabEntryRemovalDurationHistogram[] =
    "DataUse.TabModel.ExpiredInactiveTabEntryRemovalDuration";
const char kUMAExpiredActiveTabEntryRemovalDurationHistogram[] =
    "DataUse.TabModel.ExpiredActiveTabEntryRemovalDuration";
const char kUMAUnexpiredTabEntryRemovalDurationHistogram[] =
    "DataUse.TabModel.UnexpiredTabEntryRemovalDuration";

// Returns true if |tab_id| is a valid tab ID.
bool IsValidTabID(SessionID::id_type tab_id) {
  return tab_id >= 0;
}

// Returns various parameters from the values specified in the field trial.
size_t GetMaxTabEntries() {
  size_t max_tab_entries = kDefaultMaxTabEntries;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "max_tab_entries");
  if (!variation_value.empty() &&
      base::StringToSizeT(variation_value, &max_tab_entries)) {
    return max_tab_entries;
  }
  return kDefaultMaxTabEntries;
}

// Returns various parameters from the values specified in the field trial.
size_t GetMaxSessionsPerTab() {
  size_t max_sessions_per_tab = kDefaultMaxSessionsPerTab;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "max_sessions_per_tab");
  if (!variation_value.empty() &&
      base::StringToSizeT(variation_value, &max_sessions_per_tab)) {
    return max_sessions_per_tab;
  }
  return kDefaultMaxSessionsPerTab;
}

base::TimeDelta GetClosedTabExpirationDuration() {
  uint32_t duration_seconds = kDefaultClosedTabExpirationDurationSeconds;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "closed_tab_expiration_duration_seconds");
  if (!variation_value.empty() &&
      base::StringToUint(variation_value, &duration_seconds)) {
    return base::TimeDelta::FromSeconds(duration_seconds);
  }
  return base::TimeDelta::FromSeconds(
      kDefaultClosedTabExpirationDurationSeconds);
}

base::TimeDelta GetOpenTabExpirationDuration() {
  uint32_t duration_seconds = kDefaultOpenTabExpirationDurationSeconds;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "open_tab_expiration_duration_seconds");
  if (!variation_value.empty() &&
      base::StringToUint(variation_value, &duration_seconds)) {
    return base::TimeDelta::FromSeconds(duration_seconds);
  }
  return base::TimeDelta::FromSeconds(kDefaultOpenTabExpirationDurationSeconds);
}

}  // namespace

namespace chrome {

namespace android {

DataUseTabModel::DataUseTabModel(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : max_tab_entries_(GetMaxTabEntries()),
      max_sessions_per_tab_(GetMaxSessionsPerTab()),
      closed_tab_expiration_duration_(GetClosedTabExpirationDuration()),
      open_tab_expiration_duration_(GetOpenTabExpirationDuration()),
      tick_clock_(new base::DefaultTickClock()),
      ui_task_runner_(ui_task_runner),
      weak_factory_(this) {
  DCHECK(ui_task_runner_);
  data_use_matcher_.reset(new DataUseMatcher(weak_factory_.GetWeakPtr()));
}

DataUseTabModel::~DataUseTabModel() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

base::WeakPtr<DataUseTabModel> DataUseTabModel::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

void DataUseTabModel::OnNavigationEvent(SessionID::id_type tab_id,
                                        TransitionType transition,
                                        const GURL& url,
                                        const std::string& package) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidTabID(tab_id));

  TabEntryMap::const_iterator tab_entry_iterator = active_tabs_.find(tab_id);
  const std::string old_label =
      (tab_entry_iterator != active_tabs_.end())
          ? tab_entry_iterator->second.GetActiveTrackingSessionLabel()
          : std::string();
  std::string new_label;
  bool matches;

  switch (transition) {
    case TRANSITION_OMNIBOX_SEARCH:
    case TRANSITION_OMNIBOX_NAVIGATION:
      // Enter or exit events.
      if (!url.is_empty()) {
        matches = data_use_matcher_->MatchesURL(url, &new_label);
        DCHECK(matches != new_label.empty());
      }
      break;

    case TRANSITION_CUSTOM_TAB:
    case TRANSITION_LINK:
    case TRANSITION_RELOAD:
      // Enter events.
      if (!old_label.empty()) {
        new_label = old_label;
        break;
      }
      // Package name should match, for transitions from external app.
      if (transition == TRANSITION_CUSTOM_TAB && !package.empty()) {
        matches = data_use_matcher_->MatchesAppPackageName(package, &new_label);
        DCHECK_NE(matches, new_label.empty());
      }
      if (new_label.empty() && !url.is_empty()) {
        matches = data_use_matcher_->MatchesURL(url, &new_label);
        DCHECK_NE(matches, new_label.empty());
      }
      break;

    case TRANSITION_BOOKMARK:
    case TRANSITION_HISTORY_ITEM:
      // Exit events.
      DCHECK(new_label.empty());
      break;

    default:
      NOTREACHED();
      break;
  }
  if (!old_label.empty() && new_label.empty())
    EndTrackingDataUse(tab_id);
  else if (old_label.empty() && !new_label.empty())
    StartTrackingDataUse(tab_id, new_label);
}

void DataUseTabModel::OnTabCloseEvent(SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidTabID(tab_id));

  TabEntryMap::iterator tab_entry_iterator = active_tabs_.find(tab_id);
  if (tab_entry_iterator == active_tabs_.end())
    return;

  TabDataUseEntry& tab_entry = tab_entry_iterator->second;
  if (tab_entry.IsTrackingDataUse())
    tab_entry.EndTracking();
  tab_entry.OnTabCloseEvent();
}

void DataUseTabModel::OnTrackingLabelRemoved(std::string label) {
  for (auto& tab_entry : active_tabs_) {
    if (tab_entry.second.EndTrackingWithLabel(label))
      NotifyObserversOfTrackingEnding(tab_entry.first);
  }
}

bool DataUseTabModel::GetLabelForDataUse(const data_usage::DataUse& data_use,
                                         std::string* output_label) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  *output_label = "";

  // Data use that cannot be attributed to a tab will not be labeled.
  if (!IsValidTabID(data_use.tab_id))
    return false;

  TabEntryMap::const_iterator tab_entry_iterator =
      active_tabs_.find(data_use.tab_id);
  if (tab_entry_iterator != active_tabs_.end()) {
    return tab_entry_iterator->second.GetLabel(data_use.request_start,
                                               output_label);
  }

  return false;  // Tab session not found.
}

void DataUseTabModel::AddObserver(base::WeakPtr<TabDataUseObserver> observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.push_back(observer);
}

void DataUseTabModel::RegisterURLRegexes(
    const std::vector<std::string>* app_package_name,
    const std::vector<std::string>* domain_path_regex,
    const std::vector<std::string>* label) {
  data_use_matcher_->RegisterURLRegexes(app_package_name, domain_path_regex,
                                        label);
}

base::TimeTicks DataUseTabModel::NowTicks() const {
  return tick_clock_->NowTicks();
}

void DataUseTabModel::NotifyObserversOfTrackingStarting(
    SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_);
  for (const auto& observer : observers_) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::Bind(&TabDataUseObserver::NotifyTrackingStarting,
                              observer, tab_id));
  }
}

void DataUseTabModel::NotifyObserversOfTrackingEnding(
    SessionID::id_type tab_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ui_task_runner_);
  for (const auto& observer : observers_) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::Bind(&TabDataUseObserver::NotifyTrackingEnding,
                              observer, tab_id));
  }
}

void DataUseTabModel::StartTrackingDataUse(SessionID::id_type tab_id,
                                           const std::string& label) {
  // TODO(rajendrant): Explore ability to handle changes in label for current
  // session.
  bool new_tab_entry_added = false;
  TabEntryMap::iterator tab_entry_iterator = active_tabs_.find(tab_id);
  if (tab_entry_iterator == active_tabs_.end()) {
    auto new_entry = active_tabs_.insert(
        TabEntryMap::value_type(tab_id, TabDataUseEntry(this)));
    tab_entry_iterator = new_entry.first;
    DCHECK(tab_entry_iterator != active_tabs_.end());
    DCHECK(!tab_entry_iterator->second.IsTrackingDataUse());
    new_tab_entry_added = true;
  }
  if (tab_entry_iterator->second.StartTracking(label))
    NotifyObserversOfTrackingStarting(tab_id);

  if (new_tab_entry_added)
    CompactTabEntries();  // Keep total number of tab entries within limit.
}

void DataUseTabModel::EndTrackingDataUse(SessionID::id_type tab_id) {
  TabEntryMap::iterator tab_entry_iterator = active_tabs_.find(tab_id);
  if (tab_entry_iterator != active_tabs_.end() &&
      tab_entry_iterator->second.EndTracking()) {
    NotifyObserversOfTrackingEnding(tab_id);
  }
}

void DataUseTabModel::CompactTabEntries() {
  // Remove expired tab entries.
  for (TabEntryMap::iterator tab_entry_iterator = active_tabs_.begin();
       tab_entry_iterator != active_tabs_.end();) {
    const auto& tab_entry = tab_entry_iterator->second;
    if (tab_entry.IsExpired()) {
      // Track the lifetime of expired tab entry.
      const base::TimeDelta removal_time =
          NowTicks() - tab_entry.GetLatestStartOrEndTime();
      if (!tab_entry.IsTrackingDataUse()) {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            kUMAExpiredInactiveTabEntryRemovalDurationHistogram, removal_time,
            base::TimeDelta::FromSeconds(1), base::TimeDelta::FromHours(1), 50);
      } else {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            kUMAExpiredActiveTabEntryRemovalDurationHistogram, removal_time,
            base::TimeDelta::FromHours(1), base::TimeDelta::FromDays(5), 50);
      }
      active_tabs_.erase(tab_entry_iterator++);
    } else {
      ++tab_entry_iterator;
    }
  }

  if (active_tabs_.size() <= max_tab_entries_)
    return;

  // Remove oldest unexpired tab entries.
  while (active_tabs_.size() > max_tab_entries_) {
    // Find oldest tab entry.
    TabEntryMap::iterator oldest_tab_entry_iterator = active_tabs_.begin();
    for (TabEntryMap::iterator tab_entry_iterator = active_tabs_.begin();
         tab_entry_iterator != active_tabs_.end(); ++tab_entry_iterator) {
      if (oldest_tab_entry_iterator->second.GetLatestStartOrEndTime() >
          tab_entry_iterator->second.GetLatestStartOrEndTime()) {
        oldest_tab_entry_iterator = tab_entry_iterator;
      }
    }
    DCHECK(oldest_tab_entry_iterator != active_tabs_.end());
    UMA_HISTOGRAM_CUSTOM_TIMES(
        kUMAUnexpiredTabEntryRemovalDurationHistogram,
        NowTicks() -
            oldest_tab_entry_iterator->second.GetLatestStartOrEndTime(),
        base::TimeDelta::FromMinutes(1), base::TimeDelta::FromHours(1), 50);
    active_tabs_.erase(oldest_tab_entry_iterator);
  }
}

}  // namespace android

}  // namespace chrome
