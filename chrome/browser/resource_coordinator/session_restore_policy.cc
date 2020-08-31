// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/session_restore_policy.h"

#include <math.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_reader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#endif

namespace resource_coordinator {

namespace {

bool IsApp(content::WebContents* contents) {
  static constexpr char kInternalUrlPrefix[] = "chrome-extension://";
  const GURL& url = contents->GetLastCommittedURL();
  return strncmp(url.spec().c_str(), kInternalUrlPrefix,
                 base::size(kInternalUrlPrefix));
}

bool IsInternalPage(content::WebContents* contents) {
  static constexpr char kInternalUrlPrefix[] = "chrome://";
  const GURL& url = contents->GetLastCommittedURL();
  return strncmp(url.spec().c_str(), kInternalUrlPrefix,
                 base::size(kInternalUrlPrefix));
}

class SysInfoDelegate : public SessionRestorePolicy::Delegate {
 public:
  SysInfoDelegate() {}
  ~SysInfoDelegate() override {}

  size_t GetNumberOfCores() const override {
    return base::SysInfo::NumberOfProcessors();
  }

  size_t GetFreeMemoryMiB() const override {
    constexpr int64_t kMibibytesInBytes = 1 << 20;
    int64_t free_mem =
        base::SysInfo::AmountOfAvailablePhysicalMemory() / kMibibytesInBytes;
    DCHECK(free_mem >= 0);
    return free_mem;
  }

  base::TimeTicks NowTicks() const override { return base::TimeTicks::Now(); }

  size_t GetSiteEngagementScore(content::WebContents* contents) const override {
    // Get the active navigation entry. Restored tabs should always have one.
    auto& controller = contents->GetController();
    auto* nav_entry =
        controller.GetEntryAtIndex(controller.GetCurrentEntryIndex());
    DCHECK(nav_entry);

    auto* engagement_svc = SiteEngagementService::Get(
        Profile::FromBrowserContext(contents->GetBrowserContext()));
    double engagement =
        engagement_svc->GetDetails(nav_entry->GetURL()).total_score;

    // Return the engagement as an integer.
    return engagement;
  }

  static SysInfoDelegate* Get() {
    static base::NoDestructor<SysInfoDelegate> delegate;
    return delegate.get();
  }
};

}  // namespace

#if !defined(OS_ANDROID)
class TabDataAccess {
 public:
  using TabData = SessionRestorePolicy::TabData;

  // Instances of this class shouldn't be created, it should only be used via
  // its static methods.
  TabDataAccess() = delete;
  ~TabDataAccess() = delete;
  TabDataAccess(const TabDataAccess&) = delete;
  TabDataAccess& operator=(const TabDataAccess&) = delete;

  // Schedule the task that will initialize |TabData::used_in_bg| from the local
  // site characteristics database. This will either call
  // |SetUsedInBgFromSiteCharacteristicsDataReader| if the data is available
  // immediately, or will schedule a call to |OnSiteDataLoaded| once the data is
  // available.
  static void SetUsedInBgFromSiteCharacteristicsDB(
      base::WeakPtr<SessionRestorePolicy> policy,
      TabData* tab_data,
      content::WebContents* contents);

  // Set the |TabData::used_in_bg| bit based on the data provided by |reader|.
  static void SetUsedInBgFromSiteCharacteristicsDataReader(
      TabData* tab_data,
      content::WebContents* contents,
      std::unique_ptr<SiteCharacteristicsDataReader> reader);

  // Callback that is invoked when the SiteCharacteristicsDataReader associated
  // with a WebContents is ready to use. This will initialize
  // |tab_data->used_in_bg| to the proper value and call
  // |DispatchNotifyAllTabsScoredIfNeeded|.
  static void OnSiteDataLoaded(
      base::WeakPtr<SessionRestorePolicy> policy,
      content::WebContents* contents,
      std::unique_ptr<SiteCharacteristicsDataReader> reader);
};

void TabDataAccess::SetUsedInBgFromSiteCharacteristicsDB(
    base::WeakPtr<SessionRestorePolicy> policy,
    TabData* tab_data,
    content::WebContents* contents) {
  // Get a reader for the local site characteristics data corresponding to this
  // tab's origin. This makes it possible to determine if the tab has been
  // observed communicating with the user while in the background. This can be
  // ready immediately in which case a final score is emitted immediately.
  // Otherwise, it will be loaded asynchronously and the score will potentially
  // be updated.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DCHECK(profile);
  auto* factory = LocalSiteCharacteristicsDataStoreFactory::GetInstance();
  auto* store = factory->GetForProfile(profile);

  // The local database isn't always available.
  // TODO(chrisha): Note that the reader can only not exist in tests. Once we
  // have a single point to check for the availability of the performance
  // manager, gate the following logic behind that.
  if (!store) {
    tab_data->used_in_bg = false;
    return;
  }

  auto reader = store->GetReaderForOrigin(
      url::Origin::Create(contents->GetLastCommittedURL()));

  if (reader->DataLoaded()) {
    SetUsedInBgFromSiteCharacteristicsDataReader(tab_data, contents,
                                                 std::move(reader));
    DCHECK(tab_data->used_in_bg.has_value());
    if (tab_data->used_in_bg)
      ++policy->tabs_used_in_bg_;
  } else {
    auto* reader_raw = reader.get();
    tab_data->used_in_bg_setter_cancel_closure.Reset(
        base::BindOnce(&TabDataAccess::OnSiteDataLoaded, std::move(policy),
                       contents, std::move(reader)));
    reader_raw->RegisterDataLoadedCallback(
        tab_data->used_in_bg_setter_cancel_closure.callback());
  }
}

void TabDataAccess::SetUsedInBgFromSiteCharacteristicsDataReader(
    TabData* tab_data,
    content::WebContents* contents,
    std::unique_ptr<SiteCharacteristicsDataReader> reader) {
  static const performance_manager::SiteFeatureUsage kNotUsed =
      performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse;
  DCHECK(reader->DataLoaded());

  // Determine if background communication with the user is used. A pinned tab
  // has no visible tab title, so tab title updates can be ignored in that case.
  // The audio bit is ignored as tab can't play audio until they have been
  // visible at least once. We err on the side of caution, if unsure about a
  // feature (usually because of a lack of observation) then the feature is
  // considered as used.
  bool used_in_bg = reader->UpdatesFaviconInBackground() != kNotUsed;
  if (!tab_data->is_pinned && (reader->UpdatesTitleInBackground() != kNotUsed))
    used_in_bg = true;

  // TODO(sebmarchand): Consider that the tabs that are still under observation
  // could be used in background.

  auto notif_permission =
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()))
          ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS,
                                contents->GetLastCommittedURL(),
                                contents->GetLastCommittedURL());
  if (notif_permission.content_setting == CONTENT_SETTING_ALLOW)
    used_in_bg = true;

  // Persist this data and detach from the reader. We need to detach from the
  // reader in a separate task because this callback is actually being invoked
  // by the reader itself; as we're the sole owner, we'll cause it to be deleted
  // while several stack frames deep in its code, causing an immediate use
  // after free.
  tab_data->used_in_bg = used_in_bg;
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                     std::move(reader));
}

void TabDataAccess::OnSiteDataLoaded(
    base::WeakPtr<SessionRestorePolicy> policy,
    content::WebContents* contents,
    std::unique_ptr<SiteCharacteristicsDataReader> reader) {
  if (!policy)
    return;

  auto it = policy->tab_data_.find(contents);
  DCHECK(it != policy->tab_data_.end());
  auto* tab_data = it->second.get();

  SetUsedInBgFromSiteCharacteristicsDataReader(tab_data, contents,
                                               std::move(reader));

  // Score the tab and notify observers if the score has changed.
  if (policy->RescoreTabAfterDataLoaded(contents, tab_data))
    policy->notify_tab_score_changed_callback_.Run(contents, tab_data->score);

  ++policy->tabs_scored_;
  DCHECK(tab_data->used_in_bg.has_value());
  if (tab_data->used_in_bg)
    ++policy->tabs_used_in_bg_;

  policy->DispatchNotifyAllTabsScoredIfNeeded();
}
#endif

SessionRestorePolicy::SessionRestorePolicy()
    : policy_enabled_(true),
      delegate_(SysInfoDelegate::Get()),
      simultaneous_tab_loads_(CalculateSimultaneousTabLoads()) {}

SessionRestorePolicy::~SessionRestorePolicy() {
  // Record the number of tabs involved in the session restore that use
  // background communications mechanisms.
  DCHECK_GE(tabs_used_in_bg_, tabs_used_in_bg_restored_);
  UMA_HISTOGRAM_COUNTS_100("SessionRestore.BackgroundUseCaseTabCount.Total",
                           tabs_used_in_bg_);
  UMA_HISTOGRAM_COUNTS_100("SessionRestore.BackgroundUseCaseTabCount.Restored",
                           tabs_used_in_bg_restored_);
}

float SessionRestorePolicy::AddTabForScoring(content::WebContents* contents) {
  DCHECK(!base::Contains(tab_data_, contents));

  // When the first tab is added keep track of a 'now' time. This ensures that
  // the scoring function returns consistent values over the lifetime of the
  // policy object.
  if (tab_data_.empty())
    now_ = delegate_->NowTicks();

  auto iter =
      tab_data_.insert(std::make_pair(contents, std::make_unique<TabData>()));
  TabData* tab_data = iter.first->second.get();

  // Determine if the tab is pinned. This is only defined on desktop platforms.
#if defined(OS_ANDROID)
  tab_data->is_pinned = false;
#else
  // TODO(chrisha): This is O(n^2) in the number of tabs being restored. Fix
  // this!
  // In theory all tabs should belong to a tab-strip, but in tests this isn't
  // necessarily true.
  auto* browser_list = BrowserList::GetInstance();
  for (size_t i = 0; i < browser_list->size(); ++i) {
    auto* browser = browser_list->get(i);
    auto* tab_strip = browser->tab_strip_model();
    int tab_index = tab_strip->GetIndexOfWebContents(contents);
    if (tab_index == TabStripModel::kNoTab)
      continue;
    tab_data->is_pinned = tab_strip->IsTabPinned(tab_index);
    break;
  }
#endif  // !defined(OS_ANDROID)

  // Cache a handful of other properties.
  tab_data->is_app = IsApp(contents);
  tab_data->is_internal = IsInternalPage(contents);
  tab_data->site_engagement = delegate_->GetSiteEngagementScore(contents);
  tab_data->last_active = now_ - contents->GetLastActiveTime();

  // The local database doesn't exist on Android at all.
#if !defined(OS_ANDROID)
  TabDataAccess::SetUsedInBgFromSiteCharacteristicsDB(
      weak_factory_.GetWeakPtr(), tab_data, contents);
#endif  // !defined(OS_ANDROID)

  // Another tab has been added, so an existing all tabs scored notification may
  // be required.
  if (HasFinalScore(tab_data)) {
    ++tabs_scored_;
    if (notification_state_ == NotificationState::kDelivered)
      notification_state_ = NotificationState::kNotSent;
    DispatchNotifyAllTabsScoredIfNeeded();
  } else {
    notification_state_ = NotificationState::kNotSent;
  }

  ScoreTab(tab_data);
  return tab_data->score;
}

void SessionRestorePolicy::RemoveTabForScoring(content::WebContents* contents) {
  auto it = tab_data_.find(contents);
  DCHECK(it != tab_data_.end());
  auto* tab_data = it->second.get();

  if (HasFinalScore(tab_data)) {
    --tabs_scored_;

    // Tabs are removed from the policy engine when they start loading.
    if (tab_data->UsedInBg())
      ++tabs_used_in_bg_restored_;
  }

  tab_data_.erase(it);
  DispatchNotifyAllTabsScoredIfNeeded();
}

bool SessionRestorePolicy::ShouldLoad(content::WebContents* contents) const {
  // If the policy is disabled then always return true.
  if (!policy_enabled_)
    return true;

  if (tab_loads_started_ < min_tabs_to_restore_)
    return true;

  if (max_tabs_to_restore_ != 0 && tab_loads_started_ >= max_tabs_to_restore_)
    return false;

  // If there is a free memory constraint then enforce it.
  if (mb_free_memory_per_tab_to_restore_ != 0) {
    size_t free_mem_mb = delegate_->GetFreeMemoryMiB();
    if (free_mem_mb < mb_free_memory_per_tab_to_restore_)
      return false;
  }

  auto it = tab_data_.find(contents);
  DCHECK(it != tab_data_.end());
  const TabData* tab_data = it->second.get();

  // Enforce a max time since use if one is specified.
  if (!max_time_since_last_use_to_restore_.is_zero()) {
    base::TimeDelta time_since_active =
        delegate_->NowTicks() - contents->GetLastActiveTime();
    if (time_since_active > max_time_since_last_use_to_restore_)
      return false;
  }

  // Only enforce the site engagement score for tabs that don't make use of
  // background communication mechanisms. These sites often have low engagements
  // because they are only used very sporadically, but it is important that they
  // are loaded because if not loaded the user can miss important messages.
  bool enforce_site_engagement_score = true;
  if (tab_data->UsedInBg())
    enforce_site_engagement_score = false;

  // Enforce a minimum site engagement score if applicable.
  if (enforce_site_engagement_score &&
      tab_data->site_engagement < min_site_engagement_to_restore_) {
    return false;
  }

  return true;
}

void SessionRestorePolicy::NotifyTabLoadStarted() {
  ++tab_loads_started_;
}

SessionRestorePolicy::SessionRestorePolicy(bool policy_enabled,
                                           const Delegate* delegate)
    : policy_enabled_(policy_enabled),
      delegate_(delegate),
      simultaneous_tab_loads_(CalculateSimultaneousTabLoads()) {}

// static
size_t SessionRestorePolicy::CalculateSimultaneousTabLoads(
    size_t min_loads,
    size_t max_loads,
    size_t cores_per_load,
    size_t num_cores) {
  DCHECK(max_loads == 0 || min_loads <= max_loads);
  DCHECK(num_cores > 0);

  size_t loads = 0;

  // Setting |cores_per_load| == 0 means that no per-core limit is applied.
  if (cores_per_load == 0) {
    loads = std::numeric_limits<size_t>::max();
  } else {
    loads = num_cores / cores_per_load;
  }

  // If |max_loads| isn't zero then apply the maximum that it implies.
  if (max_loads != 0)
    loads = std::min(loads, max_loads);

  loads = std::max(loads, min_loads);

  return loads;
}

size_t SessionRestorePolicy::CalculateSimultaneousTabLoads() const {
  // If the policy is disabled then there are no limits on the simultaneous tab
  // loads.
  if (!policy_enabled_)
    return std::numeric_limits<size_t>::max();
  return CalculateSimultaneousTabLoads(
      min_simultaneous_tab_loads_, max_simultaneous_tab_loads_,
      cores_per_simultaneous_tab_load_, delegate_->GetNumberOfCores());
}

void SessionRestorePolicy::DispatchNotifyAllTabsScoredIfNeeded() {
  // If a notification has already been sent then there's no need to send
  // another.
  if (notification_state_ == NotificationState::kDelivered)
    return;

  if (tabs_scored_ != tab_data_.size()) {
    // An enroute notification should be canceled, as its no longer valid.
    notification_state_ = NotificationState::kNotSent;
    return;
  }

  // A notification is already enroute, no need to send another.
  if (notification_state_ == NotificationState::kEnRoute)
    return;

  // This is done asynchronously so that this notification doesn't arrive before
  // a tab score is delivered.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SessionRestorePolicy::NotifyAllTabsScored,
                                weak_factory_.GetWeakPtr()));
  notification_state_ = NotificationState::kEnRoute;
}

void SessionRestorePolicy::NotifyAllTabsScored() {
  // Only deliver the notification if its still desired; pending notifications
  // can be canceled as conditions change.
  if (notification_state_ != NotificationState::kEnRoute)
    return;
  notification_state_ = NotificationState::kDelivered;
  // This callback can indirectly cause our parent to release us, so make it the
  // last thing we do to avoid a use after free. crbug.com/946863
  notify_tab_score_changed_callback_.Run(nullptr, 0.0);
}

bool SessionRestorePolicy::RescoreTabAfterDataLoaded(
    content::WebContents* contents /* unused */,
    TabData* tab_data) {
  return ScoreTab(tab_data);
}

// static
bool SessionRestorePolicy::ScoreTab(TabData* tab_data) {
  float score = 0.0f;

  // Give higher priorities to tabs used in the background, and lowest
  // priority to internal tabs. Apps and pinned tabs are simply treated as
  // normal tabs.
  if (tab_data->UsedInBg()) {
    score = 2;
  } else if (!tab_data->is_internal) {
    score = 1;
  }

  // Refine the score using the age of the tab. More recently used tabs have
  // higher scores.
  score += CalculateAgeScore(tab_data);

  if (score == tab_data->score)
    return false;

  tab_data->score = score;
  return true;
}

// static
float SessionRestorePolicy::CalculateAgeScore(const TabData* tab_data) {
  // Convert the age into seconds. Cap absolute values less than 1 so that
  // the inverse will be between -1 and 1.
  double score = tab_data->last_active.InSecondsF();
  if (fabs(score) < 1.0f) {
    if (score > 0)
      score = 1;
    else
      score = -1;
  }
  DCHECK_LE(1.0f, fabs(score));

  // Invert the score (1 / score).
  // Really old (infinity) maps to 0 (lowest priority).
  // Really young positive age (1) maps to 1 (moderate priority).
  // A little in the future (-1) maps to -1 (moderate priority).
  // Really far in the future (-infinity) maps to 0 (highest priority).
  // Shifting negative scores from [-1, 0] to [1, 2] keeps the scores increasing
  // with priority.
  if (score < 0) {
    score = 2.0 + 1.0 / score;
  } else {
    score = 1.0 / score;
  }
  DCHECK_LE(0.0, score);
  DCHECK_GE(2.0, score);

  // Rescale the age score to the range [0, 1] so that it can be added to the
  // category scores already calculated. Divide by 2 + epsilon so that no
  // score will end up rounding up to 1.0, but instead be capped at 0.999.
  score /= 2.002;
  DCHECK_LE(0.0, score);
  DCHECK_GT(1.0, score);

  return score;
}

// static
bool SessionRestorePolicy::HasFinalScore(const TabData* tab_data) {
  return tab_data->used_in_bg.has_value();
}

void SessionRestorePolicy::SetTabLoadsStartedForTesting(
    size_t tab_loads_started) {
  tab_loads_started_ = tab_loads_started;
}

void SessionRestorePolicy::UpdateSiteEngagementScoreForTesting(
    content::WebContents* contents,
    size_t score) {
  auto it = tab_data_.find(contents);
  it->second->site_engagement = score;
}

SessionRestorePolicy::Delegate::Delegate() {}

SessionRestorePolicy::Delegate::~Delegate() {}

SessionRestorePolicy::TabData::TabData() = default;

SessionRestorePolicy::TabData::~TabData() {
  used_in_bg_setter_cancel_closure.Cancel();
}

bool SessionRestorePolicy::TabData::UsedInBg() const {
  return used_in_bg.has_value() && used_in_bg.value();
}

}  // namespace resource_coordinator
