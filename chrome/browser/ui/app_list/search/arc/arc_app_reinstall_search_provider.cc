// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_search_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_app_result.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/common/url_icon_source.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "extensions/grit/extensions_browser_resources.h"

namespace {
// Seconds in between refreshes;
constexpr int64_t kRefreshSeconds = 1800;
constexpr char kAppListLatency[] = "Apps.AppListRecommendedResponse.Latency";
constexpr char kAppListCounts[] = "Apps.AppListRecommendedResponse.Count";

void RecordUmaResponseParseResult(arc::mojom::AppReinstallState result) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListRecommendedResponse", result);
}

std::string LimitIconSizeWithFife(const std::string& icon_url,
                                  int icon_dimension) {
  // We append a suffix to icon url
  DCHECK_GT(icon_dimension, 0);
  return base::StrCat({icon_url, "=s", base::NumberToString(icon_dimension)});
}

}  // namespace

namespace app_list {

ArcAppReinstallSearchProvider::ArcAppReinstallSearchProvider(
    Profile* profile,
    unsigned int max_result_count)
    : profile_(profile),
      max_result_count_(max_result_count),
      icon_dimension_(
          app_list::AppListConfig::instance().GetPreferredIconDimension(
              ash::SearchResultDisplayType::kRecommendation)),
      app_fetch_timer_(std::make_unique<base::RepeatingTimer>()),
      weak_ptr_factory_(this) {
  DCHECK(profile_ != nullptr);
  ArcAppListPrefs::Get(profile_)->AddObserver(this);
  MaybeUpdateFetching();
}

ArcAppReinstallSearchProvider::~ArcAppReinstallSearchProvider() {
  ArcAppListPrefs::Get(profile_)->RemoveObserver(this);
}

void ArcAppReinstallSearchProvider::BeginRepeatingFetch() {
  // If already running, do not re-start.
  if (app_fetch_timer_->IsRunning())
    return;

  app_fetch_timer_->Start(FROM_HERE,
                          base::TimeDelta::FromSeconds(kRefreshSeconds), this,
                          &ArcAppReinstallSearchProvider::StartFetch);
  StartFetch();
}

void ArcAppReinstallSearchProvider::StopRepeatingFetch() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  app_fetch_timer_->AbandonAndStop();
  loaded_value_.clear();
  icon_urls_.clear();
  loading_icon_urls_.clear();
  UpdateResults();
}

void ArcAppReinstallSearchProvider::Start(const base::string16& query) {
  if (query_is_empty_ == query.empty())
    return;

  query_is_empty_ = query.empty();
  UpdateResults();
}

void ArcAppReinstallSearchProvider::StartFetch() {
  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetAppReinstallCandidates)
          : nullptr;
  if (app_instance == nullptr)
    return;

  app_instance->GetAppReinstallCandidates(base::BindOnce(
      &ArcAppReinstallSearchProvider::OnGetAppReinstallCandidates,
      weak_ptr_factory_.GetWeakPtr(), base::Time::Now()));
}

void ArcAppReinstallSearchProvider::OnGetAppReinstallCandidates(
    base::Time start_time,
    arc::mojom::AppReinstallState state,
    std::vector<arc::mojom::AppReinstallCandidatePtr> results) {
  RecordUmaResponseParseResult(state);
  DCHECK_NE(start_time, base::Time::UnixEpoch());
  UMA_HISTOGRAM_TIMES(kAppListLatency, base::Time::Now() - start_time);
  UMA_HISTOGRAM_COUNTS_100(kAppListCounts, results.size());

  if (state != arc::mojom::AppReinstallState::REQUEST_SUCCESS) {
    LOG(ERROR) << "Failed to get reinstall candidates: " << state;
    return;
  }
  loaded_value_.clear();

  for (const auto& candidate : results) {
    // only keep candidates with icons.
    if (candidate->icon_url != base::nullopt) {
      loaded_value_.push_back(candidate.Clone());
    }
  }
  UpdateResults();
}

void ArcAppReinstallSearchProvider::UpdateResults() {
  // We clear results if there are none from the server, or the user has entered
  // a non-zero query.
  if (loaded_value_.empty() || !query_is_empty_) {
    if (loaded_value_.empty())
      icon_urls_.clear();
    ClearResults();
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(prefs != nullptr);

  std::vector<std::unique_ptr<ChromeSearchResult>> new_results;
  std::unordered_set<std::string> used_icon_urls;

  // Lock over the whole list.
  for (size_t i = 0, processed = 0;
       i < loaded_value_.size() && processed < max_result_count_; ++i) {
    // Any packages that are installing or installed and not in sync with the
    // server are removed with IsUnknownPackage.
    if (!prefs->IsUnknownPackage(loaded_value_[i]->package_name))
      continue;

    processed++;

    // From this point, we believe that this item should be in the result list.
    // We try to find this icon, and if it is not available, we load it.
    const std::string& icon_url = loaded_value_[i]->icon_url.value();
    // All the icons we are showing.
    used_icon_urls.insert(icon_url);

    const auto icon_it = icon_urls_.find(icon_url);
    const auto loading_icon_it = loading_icon_urls_.find(icon_url);
    if (icon_it == icon_urls_.end() &&
        loading_icon_it == loading_icon_urls_.end()) {
      // this icon is not loaded, nor is it in the loading set. Add it.
      loading_icon_urls_[icon_url] = gfx::ImageSkia(
          std::make_unique<app_list::UrlIconSource>(
              base::BindRepeating(&ArcAppReinstallSearchProvider::OnIconLoaded,
                                  weak_ptr_factory_.GetWeakPtr(), icon_url),
              profile_, GURL(LimitIconSizeWithFife(icon_url, icon_dimension_)),
              icon_dimension_, IDR_APP_DEFAULT_ICON),
          gfx::Size(icon_dimension_, icon_dimension_));
      loading_icon_urls_[icon_url].GetRepresentation(1.0f);
    } else if (icon_it != icon_urls_.end()) {
      // Icon is loaded, add it to the results.
      new_results.emplace_back(std::make_unique<ArcAppReinstallAppResult>(
          loaded_value_[i], icon_it->second));
    }
  }

  // Remove unused icons.
  std::unordered_set<std::string> unused_icon_urls;
  for (const auto& it : icon_urls_) {
    if (used_icon_urls.find(it.first) == used_icon_urls.end()) {
      // This url is used, remove.
      unused_icon_urls.insert(it.first);
    }
  }

  for (const std::string& url : unused_icon_urls) {
    icon_urls_.erase(url);
    loading_icon_urls_.erase(url);
  }

  // Now we are ready with new_results. do we actually need to replace things on
  // screen?
  if (!ResultsIdentical(results(), new_results)) {
    SwapResults(&new_results);
  }
}

void ArcAppReinstallSearchProvider::MaybeUpdateFetching() {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(arc::kPlayStoreAppId);
  if (app_info && app_info->ready)
    BeginRepeatingFetch();
  else
    StopRepeatingFetch();
}

void ArcAppReinstallSearchProvider::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  OnAppStatesChanged(app_id, app_info);
}

void ArcAppReinstallSearchProvider::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id == arc::kPlayStoreAppId)
    MaybeUpdateFetching();
}

void ArcAppReinstallSearchProvider::OnAppRemoved(const std::string& app_id) {
  if (app_id == arc::kPlayStoreAppId)
    MaybeUpdateFetching();
}

void ArcAppReinstallSearchProvider::OnInstallationStarted(
    const std::string& package_name) {
  UpdateResults();
}
void ArcAppReinstallSearchProvider::OnInstallationFinished(
    const std::string& package_name,
    bool success) {
  UpdateResults();
}

void ArcAppReinstallSearchProvider::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  UpdateResults();
}

void ArcAppReinstallSearchProvider::OnPackageRemoved(
    const std::string& package_name,
    bool uninstalled) {
  UpdateResults();
}

void ArcAppReinstallSearchProvider::SetTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  app_fetch_timer_ = std::move(timer);
}

// For icon load callback, in OnGetAppReinstallCandidates
void ArcAppReinstallSearchProvider::OnIconLoaded(const std::string& icon_url) {
  auto skia_ptr = loading_icon_urls_.find(icon_url);

  DCHECK(skia_ptr != loading_icon_urls_.end());
  if (skia_ptr == loading_icon_urls_.end()) {
    return;
  }
  const std::vector<gfx::ImageSkiaRep> image_reps =
      skia_ptr->second.image_reps();
  for (const gfx::ImageSkiaRep& rep : image_reps)
    skia_ptr->second.RemoveRepresentation(rep.scale());
  DCHECK_LE(skia_ptr->second.width(), icon_dimension_);

  // ImageSkia is now ready to serve, move to the done list and update the
  // screen.
  icon_urls_[icon_url] = skia_ptr->second;
  loading_icon_urls_.erase(icon_url);
  UpdateResults();
}

bool ArcAppReinstallSearchProvider::ResultsIdentical(
    const std::vector<std::unique_ptr<ChromeSearchResult>>& old_results,
    const std::vector<std::unique_ptr<ChromeSearchResult>>& new_results) {
  if (old_results.size() != new_results.size()) {
    return false;
  }
  for (size_t i = 0; i < old_results.size(); ++i) {
    const ChromeSearchResult& old_result = *(old_results[i]);
    const ChromeSearchResult& new_result = *(new_results[i]);
    if (!old_result.icon().BackedBySameObjectAs(new_result.icon())) {
      return false;
    }
    if (old_result.title() != new_result.title()) {
      return false;
    }
    if (old_result.id() != new_result.id()) {
      return false;
    }
    if (old_result.relevance() != new_result.relevance()) {
      return false;
    }
    if (old_result.rating() != new_result.rating()) {
      return false;
    }
  }
  return true;
}

}  // namespace app_list
