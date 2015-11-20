// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_model.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "chrome/browser/android/data_usage/tab_data_use_entry.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Indicates the default maximum number of tabs to maintain session information
// about. May be overridden by the field trial.
const size_t kDefaultMaxTabEntries = 200;

const char kUMAExpiredInactiveTabEntryRemovalDurationSecondsHistogram[] =
    "DataUse.TabModel.ExpiredInactiveTabEntryRemovalDuration";
const char kUMAExpiredActiveTabEntryRemovalDurationHoursHistogram[] =
    "DataUse.TabModel.ExpiredActiveTabEntryRemovalDuration";
const char kUMAUnexpiredTabEntryRemovalDurationMinutesHistogram[] =
    "DataUse.TabModel.UnexpiredTabEntryRemovalDuration";

// Returns true if |tab_id| is a valid tab ID.
bool IsValidTabID(int32_t tab_id) {
  return tab_id >= 0;
}

// Returns various parameters from the values specified in the field trial.
size_t GetMaxTabEntries() {
  size_t max_tab_entries = -1;
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

}  // namespace

namespace chrome {

namespace android {

DataUseTabModel::DataUseTabModel(
    const ExternalDataUseObserver* data_use_observer,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : data_use_observer_(data_use_observer),
      observer_list_(new base::ObserverListThreadSafe<TabDataUseObserver>),
      max_tab_entries_(GetMaxTabEntries()),
      weak_factory_(this) {}

DataUseTabModel::~DataUseTabModel() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

base::WeakPtr<DataUseTabModel> DataUseTabModel::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

void DataUseTabModel::OnNavigationEvent(int32_t tab_id,
                                        TransitionType transition,
                                        const GURL& url,
                                        const std::string& package) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidTabID(tab_id));

  switch (transition) {
    case TRANSITION_OMNIBOX_SEARCH:
    case TRANSITION_FROM_EXTERNAL_APP: {
      // Enter events.
      bool start_tracking = false;
      std::string label;
      TabEntryMap::const_iterator tab_entry_iterator =
          active_tabs_.find(tab_id);
      if (tab_entry_iterator != active_tabs_.end() &&
          tab_entry_iterator->second.IsTrackingDataUse()) {
        break;
      }
      if (transition == TRANSITION_FROM_EXTERNAL_APP) {
        // Package name should match, for transitions from external app.
        if (!package.empty() &&
            data_use_observer_->MatchesAppPackageName(package, &label)) {
          DCHECK(!label.empty());
          start_tracking = true;
        }
      }
      if (!start_tracking && !url.is_empty() &&
          data_use_observer_->Matches(url, &label)) {
        DCHECK(!label.empty());
        start_tracking = true;
      }
      if (start_tracking)
        StartTrackingDataUse(tab_id, label);
      break;
    }

    case TRANSITION_FROM_NAVSUGGEST:
    case TRANSITION_OMNIBOX_NAVIGATION:
    case TRANSITION_BOOKMARK:
    case TRANSITION_HISTORY_ITEM:
    case TRANSITION_TO_EXTERNAL_APP:
      // Exit events.
      EndTrackingDataUse(tab_id);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void DataUseTabModel::OnTabCloseEvent(int32_t tab_id) {
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

void DataUseTabModel::AddObserver(TabDataUseObserver* observer) {
  observer_list_->AddObserver(observer);
}

void DataUseTabModel::RemoveObserver(TabDataUseObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

base::TimeTicks DataUseTabModel::Now() const {
  return base::TimeTicks::Now();
}

void DataUseTabModel::NotifyObserversOfTrackingStarting(int32_t tab_id) {
  observer_list_->Notify(FROM_HERE, &TabDataUseObserver::NotifyTrackingStarting,
                         tab_id);
}

void DataUseTabModel::NotifyObserversOfTrackingEnding(int32_t tab_id) {
  observer_list_->Notify(FROM_HERE, &TabDataUseObserver::NotifyTrackingEnding,
                         tab_id);
}

void DataUseTabModel::StartTrackingDataUse(int32_t tab_id,
                                           const std::string& label) {
  // TODO(rajendrant): Explore ability to handle changes in label for current
  // session.
  bool new_tab_entry_added = false;
  TabEntryMap::iterator tab_entry_iterator = active_tabs_.find(tab_id);
  if (tab_entry_iterator == active_tabs_.end()) {
    auto new_entry =
        active_tabs_.insert(TabEntryMap::value_type(tab_id, TabDataUseEntry()));
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

void DataUseTabModel::EndTrackingDataUse(int32_t tab_id) {
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
          Now() - tab_entry.GetLatestStartOrEndTime();
      if (!tab_entry.IsTrackingDataUse()) {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            kUMAExpiredInactiveTabEntryRemovalDurationSecondsHistogram,
            removal_time, base::TimeDelta::FromSeconds(1),
            base::TimeDelta::FromHours(1), 50);
      } else {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            kUMAExpiredActiveTabEntryRemovalDurationHoursHistogram,
            removal_time, base::TimeDelta::FromHours(1),
            base::TimeDelta::FromDays(5), 50);
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
        kUMAUnexpiredTabEntryRemovalDurationMinutesHistogram,
        Now() - oldest_tab_entry_iterator->second.GetLatestStartOrEndTime(),
        base::TimeDelta::FromMinutes(1), base::TimeDelta::FromHours(1), 50);
    active_tabs_.erase(oldest_tab_entry_iterator);
  }
}

}  // namespace android

}  // namespace chrome
