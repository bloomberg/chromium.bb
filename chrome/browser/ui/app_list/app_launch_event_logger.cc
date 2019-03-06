// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_launch_event_logger.h"

#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/recent_events_counter.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger_helpers.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/app_source_url_recorder.h"
#include "extensions/common/extension.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace app_list {

const base::Feature kUkmAppLaunchEventLogging{"UkmAppLaunchEventLogging",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Keys for Arc app specific preferences. Defined in
// chrome/browser/ui/app_list/arc/arc_app_list_prefs.cc.
const char AppLaunchEventLogger::kPackageName[] = "package_name";
const char AppLaunchEventLogger::kShouldSync[] = "should_sync";

namespace {

constexpr unsigned int kNumRandomAppsToLog = 5;
const char kArcScheme[] = "arc://";
const char kExtensionSchemeWithDelimiter[] = "chrome-extension://";

constexpr base::TimeDelta kHourDuration = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kDayDuration = base::TimeDelta::FromDays(1);
constexpr int kMinutesInAnHour = 60;
constexpr int kQuarterHoursInADay = 24 * 4;
constexpr float kTotalHoursBucketSizeMultiplier = 1.25;

constexpr std::array<chromeos::power::ml::Bucket, 2> kClickBuckets = {
    {{20, 1}, {200, 10}}};
constexpr std::array<chromeos::power::ml::Bucket, 6>
    kTimeSinceLastClickBuckets = {{{60, 1},
                                   {600, 60},
                                   {1200, 300},
                                   {3600, 600},
                                   {18000, 1800},
                                   {86400, 3600}}};

// Returns the nearest bucket for |value|, where bucket sizes are determined
// exponentially, with each bucket size increasing by a factor of |base|.
// The return value is rounded to the nearest integer.
int ExponentialBucket(int value, float base) {
  if (base <= 0) {
    LOG(DFATAL) << "Base of exponential must be positive.";
    return 0;
  }
  if (value <= 0) {
    return 0;
  }
  return round(pow(base, round(log(value) / log(base))));
}

// Selects a random sample of size |sample_size| from |population|.
std::vector<std::string> Sample(const std::vector<std::string>& population,
                                unsigned int sample_size) {
  std::vector<std::string> sample;
  unsigned int index = 0;
  // Reservoir sampling.
  for (const std::string& candidate : population) {
    if (index < sample_size) {
      sample.push_back(candidate);
    } else {
      const uint64_t r = base::RandGenerator(index + 1);
      if (r < sample_size) {
        sample[r] = candidate;
      }
    }
    index++;
  }
  return sample;
}

int HourOfDay(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.hour;
}

int DayOfWeek(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.day_of_week;
}

}  // namespace

AppLaunchEventLogger::AppLaunchEventLogger()
    : start_time_(base::Time::Now()),
      all_clicks_last_hour_(
          std::make_unique<chromeos::power::ml::RecentEventsCounter>(
              kHourDuration,
              kMinutesInAnHour)),
      all_clicks_last_24_hours_(
          std::make_unique<chromeos::power::ml::RecentEventsCounter>(
              kDayDuration,
              kQuarterHoursInADay)),
      weak_factory_(this) {
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  PopulatePwaIdUrlMap();
}

AppLaunchEventLogger::~AppLaunchEventLogger() {}

void AppLaunchEventLogger::OnSuggestionChipOrSearchBoxClicked(
    const std::string& id,
    int suggestion_index,
    int launched_from) {
  if (!base::FeatureList::IsEnabled(kUkmAppLaunchEventLogging)) {
    return;
  }
  AppLaunchEvent event;
  AppLaunchEvent_LaunchedFrom from(
      static_cast<AppLaunchEvent_LaunchedFrom>(launched_from));
  if (from == AppLaunchEvent_LaunchedFrom_SUGGESTION_CHIP ||
      from == AppLaunchEvent_LaunchedFrom_SEARCH_BOX) {
    event.set_launched_from(from);
  } else {
    return;
  }
  event.set_app_id(RemoveScheme(id));
  event.set_index(suggestion_index);
  EnforceLoggingPolicy();

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AppLaunchEventLogger::Log,
                                        weak_factory_.GetWeakPtr(), event));
}

void AppLaunchEventLogger::OnGridClicked(const std::string& id) {
  if (!base::FeatureList::IsEnabled(kUkmAppLaunchEventLogging)) {
    return;
  }
  AppLaunchEvent event;
  event.set_launched_from(AppLaunchEvent_LaunchedFrom_GRID);
  event.set_app_id(id);
  EnforceLoggingPolicy();

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AppLaunchEventLogger::Log,
                                        weak_factory_.GetWeakPtr(), event));
}

void AppLaunchEventLogger::SetAppDataForTesting(
    extensions::ExtensionRegistry* registry,
    base::DictionaryValue* arc_apps,
    base::DictionaryValue* arc_packages) {
  testing_ = true;
  registry_ = registry;
  arc_apps_ = arc_apps;
  arc_packages_ = arc_packages;
}

std::string AppLaunchEventLogger::RemoveScheme(const std::string& id) {
  std::string app_id(id);
  if (!app_id.compare(0, strlen(kExtensionSchemeWithDelimiter),
                      kExtensionSchemeWithDelimiter)) {
    app_id.erase(0, strlen(kExtensionSchemeWithDelimiter));
  }
  if (!app_id.compare(0, strlen(kArcScheme), kArcScheme)) {
    app_id.erase(0, strlen(kArcScheme));
  }
  if (app_id.size() && !app_id.compare(app_id.size() - 1, 1, "/")) {
    app_id.pop_back();
  }
  return app_id;
}

void AppLaunchEventLogger::PopulatePwaIdUrlMap() {
  pwa_id_url_map_ = base::flat_map<std::string, std::string>{
      // Google+
      {"dcdbodpaldbchkfinnjphocleggfceip", "https://plus.google.com/discover"},
      // Photos
      {"ncmjhecbjeaamljdfahankockkkdmedg", "https://photos.google.com/"}};
}

const std::string& AppLaunchEventLogger::GetPwaUrl(const std::string& id) {
  auto search = pwa_id_url_map_.find(id);
  if (search != pwa_id_url_map_.end()) {
    return search->second;
  }
  return base::EmptyString();
}

void AppLaunchEventLogger::OkApp(AppLaunchEvent_AppType app_type,
                                 const std::string& app_id,
                                 const std::string& arc_package_name,
                                 const std::string& pwa_url) {
  if (app_features_map_.find(app_id) == app_features_map_.end()) {
    AppLaunchFeatures app_launch_features;
    app_launch_features.set_app_id(app_id);
    app_launch_features.set_app_type(app_type);
    if (app_type == AppLaunchEvent_AppType_PWA) {
      app_launch_features.set_pwa_url(pwa_url);
    } else if (app_type == AppLaunchEvent_AppType_PLAY) {
      app_launch_features.set_arc_package_name(arc_package_name);
    }
    app_features_map_[app_id] = app_launch_features;
  }
  app_features_map_[app_id].set_is_policy_compliant(true);
}

void AppLaunchEventLogger::EnforceLoggingPolicy() {
  // Tests provide installed app information, so don't overwrite that.
  if (!testing_) {
    Profile* profile = ProfileManager::GetLastUsedProfile();
    if (!profile) {
      LOG(DFATAL) << "No profile";
      return;
    }
    registry_ = extensions::ExtensionRegistry::Get(profile);

    PrefService* pref_service = profile->GetPrefs();
    if (pref_service) {
      arc_apps_ = pref_service->GetDictionary(arc::prefs::kArcApps);
      arc_packages_ = pref_service->GetDictionary(arc::prefs::kArcPackages);
    }
  }

  for (auto& app : app_features_map_) {
    app.second.set_is_policy_compliant(false);
  }

  // Store all Chrome and PWA apps.
  // registry_ can be nullptr in tests.
  if (registry_) {
    std::unique_ptr<extensions::ExtensionSet> extensions =
        registry_->GenerateInstalledExtensionsSet();
    for (const auto& extension : *extensions) {
      // Only allow Chrome apps that are from the webstore.
      if (extension->from_webstore()) {
        OkApp(AppLaunchEvent_AppType_CHROME, extension->id(),
              base::EmptyString(), base::EmptyString());
      } else {
        // Only allow PWA apps on the whitelist.
        auto search = pwa_id_url_map_.find(extension->id());
        if (search != pwa_id_url_map_.end()) {
          OkApp(AppLaunchEvent_AppType_PWA, extension->id(),
                base::EmptyString(), search->second);
        }
      }
    }
  }

  // Store all Arc apps.
  // arc_apps_ and arc_packages_ can be nullptr in tests.
  if (arc_apps_ && arc_packages_) {
    for (const auto& app : arc_apps_->DictItems()) {
      const base::Value* package_name_value = app.second.FindKey(kPackageName);
      if (!package_name_value) {
        continue;
      }
      const base::Value* package =
          arc_packages_->FindKey(package_name_value->GetString());
      // Only allow Arc apps with sync enabled.
      if (!package || !package->FindKey(kShouldSync)->GetBool()) {
        continue;
      }
      OkApp(AppLaunchEvent_AppType_PLAY, app.first,
            package_name_value->GetString(), base::EmptyString());
    }
  }
  // Remove any apps that are no longer installed or no longer satisfy logging
  // policy.
  base::EraseIf(app_features_map_,
                [](const std::pair<std::string, AppLaunchFeatures>& pair) {
                  return !pair.second.is_policy_compliant();
                });
}

void AppLaunchEventLogger::ProcessClick(const AppLaunchEvent& event,
                                        const base::Time& now) {
  auto search = app_features_map_.find(event.app_id());
  if (search == app_features_map_.end()) {
    return;
  }
  for (auto& app : app_features_map_) {
    // Advance mru index for apps previously clicked on.
    if (app.second.has_most_recently_used_index()) {
      app.second.set_most_recently_used_index(
          app.second.most_recently_used_index() + 1);
    }
  }
  const base::TimeDelta duration = now - start_time_;
  AppLaunchFeatures* app_launch_features = &search->second;
  if (!app_launch_features->has_most_recently_used_index()) {
    // Handle first click on an id.
    app_clicks_last_hour_[event.app_id()] =
        std::make_unique<chromeos::power::ml::RecentEventsCounter>(
            kHourDuration, kMinutesInAnHour);
    app_clicks_last_24_hours_[event.app_id()] =
        std::make_unique<chromeos::power::ml::RecentEventsCounter>(
            kDayDuration, kQuarterHoursInADay);
    for (int hour = 0; hour < 24; hour++) {
      app_launch_features->add_clicks_each_hour(0);
    }
  }
  app_launch_features->set_most_recently_used_index(0);
  app_launch_features->set_last_launched_from(event.launched_from());
  app_launch_features->set_total_clicks(app_launch_features->total_clicks() +
                                        1);
  app_launch_features->set_time_of_last_click_sec(
      now.ToDeltaSinceWindowsEpoch().InSeconds());
  const int hour = HourOfDay(now);
  app_launch_features->set_clicks_each_hour(
      hour, app_launch_features->clicks_each_hour(hour) + 1);
  app_clicks_last_hour_[event.app_id()]->Log(duration);
  app_clicks_last_24_hours_[event.app_id()]->Log(duration);
  app_launch_features->set_clicks_last_hour(
      app_clicks_last_hour_[event.app_id()]->GetTotal(duration));
  app_launch_features->set_clicks_last_24_hours(
      app_clicks_last_24_hours_[event.app_id()]->GetTotal(duration));
}

ukm::SourceId AppLaunchEventLogger::GetSourceId(
    AppLaunchEvent_AppType app_type,
    const std::string& app_id,
    const std::string& arc_package_name,
    const std::string& pwa_url) {
  if (app_type == AppLaunchEvent_AppType_CHROME) {
    return ukm::AppSourceUrlRecorder::GetSourceIdForChromeApp(app_id);
  } else if (app_type == AppLaunchEvent_AppType_PWA) {
    return ukm::AppSourceUrlRecorder::GetSourceIdForPWA(GURL(pwa_url));
  } else if (app_type == AppLaunchEvent_AppType_PLAY) {
    return ukm::AppSourceUrlRecorder::GetSourceIdForArc(arc_package_name);
  } else {
    // Either app is Crostini; or Chrome but not in app store; or Arc but not
    // syncable; or PWA but not in whitelist.
    return ukm::kInvalidSourceId;
  }
}

std::vector<std::string> AppLaunchEventLogger::ChooseAppsToLog(
    const std::string clicked_app_id) {
  bool has_clicked_app = false;
  std::vector<std::string> clicked_apps;
  std::vector<std::string> unclicked_apps;
  // Group apps by whether they have been clicked on. Do not include the
  // currently clicked app.
  for (auto& app : app_features_map_) {
    if (app.first == clicked_app_id) {
      has_clicked_app = true;
      continue;
    }
    if (app.second.has_most_recently_used_index()) {
      clicked_apps.push_back(app.first);
    } else {
      unclicked_apps.push_back(app.first);
    }
  }

  std::vector<std::string> apps(Sample(clicked_apps, kNumRandomAppsToLog));
  if (apps.size() < kNumRandomAppsToLog) {
    std::vector<std::string> unclicked_sample(
        Sample(unclicked_apps, kNumRandomAppsToLog - apps.size()));
    apps.insert(apps.end(), unclicked_sample.begin(), unclicked_sample.end());
  }
  if (has_clicked_app) {
    apps.push_back(clicked_app_id);
  }
  return apps;
}

void AppLaunchEventLogger::RecordAppTypeClicked(
    AppLaunchEvent_AppType app_type) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListAppTypeClicked", app_type,
                            AppLaunchEvent_AppType_AppType_ARRAYSIZE);
}

void AppLaunchEventLogger::LogClicksEachHour(
    const AppLaunchFeatures& app_launch_features,
    ukm::builders::AppListAppClickData* const app_click_data) {
  int bucketized_clicks_each_hour[24];
  for (int hour = 0; hour < 24; hour++) {
    bucketized_clicks_each_hour[hour] =
        Bucketize(app_launch_features.clicks_each_hour(hour), kClickBuckets);
  }
  if (bucketized_clicks_each_hour[0] != 0) {
    app_click_data->SetClicksEachHour00(bucketized_clicks_each_hour[0]);
  }
  if (bucketized_clicks_each_hour[1] != 0) {
    app_click_data->SetClicksEachHour01(bucketized_clicks_each_hour[1]);
  }
  if (bucketized_clicks_each_hour[2] != 0) {
    app_click_data->SetClicksEachHour02(bucketized_clicks_each_hour[2]);
  }
  if (bucketized_clicks_each_hour[3] != 0) {
    app_click_data->SetClicksEachHour03(bucketized_clicks_each_hour[3]);
  }
  if (bucketized_clicks_each_hour[4] != 0) {
    app_click_data->SetClicksEachHour04(bucketized_clicks_each_hour[4]);
  }
  if (bucketized_clicks_each_hour[5] != 0) {
    app_click_data->SetClicksEachHour05(bucketized_clicks_each_hour[5]);
  }
  if (bucketized_clicks_each_hour[6] != 0) {
    app_click_data->SetClicksEachHour06(bucketized_clicks_each_hour[6]);
  }
  if (bucketized_clicks_each_hour[7] != 0) {
    app_click_data->SetClicksEachHour07(bucketized_clicks_each_hour[7]);
  }
  if (bucketized_clicks_each_hour[8] != 0) {
    app_click_data->SetClicksEachHour08(bucketized_clicks_each_hour[8]);
  }
  if (bucketized_clicks_each_hour[9] != 0) {
    app_click_data->SetClicksEachHour09(bucketized_clicks_each_hour[9]);
  }
  if (bucketized_clicks_each_hour[10] != 0) {
    app_click_data->SetClicksEachHour10(bucketized_clicks_each_hour[10]);
  }
  if (bucketized_clicks_each_hour[11] != 0) {
    app_click_data->SetClicksEachHour11(bucketized_clicks_each_hour[11]);
  }
  if (bucketized_clicks_each_hour[12] != 0) {
    app_click_data->SetClicksEachHour12(bucketized_clicks_each_hour[12]);
  }
  if (bucketized_clicks_each_hour[13] != 0) {
    app_click_data->SetClicksEachHour13(bucketized_clicks_each_hour[13]);
  }
  if (bucketized_clicks_each_hour[14] != 0) {
    app_click_data->SetClicksEachHour14(bucketized_clicks_each_hour[14]);
  }
  if (bucketized_clicks_each_hour[15] != 0) {
    app_click_data->SetClicksEachHour15(bucketized_clicks_each_hour[15]);
  }
  if (bucketized_clicks_each_hour[16] != 0) {
    app_click_data->SetClicksEachHour16(bucketized_clicks_each_hour[16]);
  }
  if (bucketized_clicks_each_hour[17] != 0) {
    app_click_data->SetClicksEachHour17(bucketized_clicks_each_hour[17]);
  }
  if (bucketized_clicks_each_hour[18] != 0) {
    app_click_data->SetClicksEachHour18(bucketized_clicks_each_hour[18]);
  }
  if (bucketized_clicks_each_hour[19] != 0) {
    app_click_data->SetClicksEachHour19(bucketized_clicks_each_hour[19]);
  }
  if (bucketized_clicks_each_hour[20] != 0) {
    app_click_data->SetClicksEachHour20(bucketized_clicks_each_hour[20]);
  }
  if (bucketized_clicks_each_hour[21] != 0) {
    app_click_data->SetClicksEachHour21(bucketized_clicks_each_hour[21]);
  }
  if (bucketized_clicks_each_hour[22] != 0) {
    app_click_data->SetClicksEachHour22(bucketized_clicks_each_hour[22]);
  }
  if (bucketized_clicks_each_hour[23] != 0) {
    app_click_data->SetClicksEachHour23(bucketized_clicks_each_hour[23]);
  }
}

void AppLaunchEventLogger::Log(AppLaunchEvent app_launch_event) {
  auto app = app_features_map_.find(app_launch_event.app_id());
  if (app == app_features_map_.end()) {
    RecordAppTypeClicked(AppLaunchEvent_AppType_OTHER);
    return;
  }
  RecordAppTypeClicked(app->second.app_type());
  ukm::SourceId launch_source_id =
      GetSourceId(app->second.app_type(), app_launch_event.app_id(),
                  app->second.arc_package_name(), app->second.pwa_url());
  if (launch_source_id == ukm::kInvalidSourceId) {
    return;
  }
  ukm::builders::AppListAppLaunch app_launch(launch_source_id);

  base::Time now(base::Time::Now());
  const base::TimeDelta duration = now - start_time_;
  all_clicks_last_hour_->Log(duration);
  all_clicks_last_24_hours_->Log(duration);

  if (app_launch_event.launched_from() ==
          AppLaunchEvent_LaunchedFrom_SUGGESTION_CHIP ||
      app_launch_event.launched_from() ==
          AppLaunchEvent_LaunchedFrom_SEARCH_BOX) {
    app_launch.SetPositionIndex(app_launch_event.index());
  }
  app_launch.SetAppType(app->second.app_type())
      .SetLaunchedFrom(app_launch_event.launched_from())
      .SetDayOfWeek(DayOfWeek(now))
      .SetHourOfDay(HourOfDay(now))
      .SetAllClicksLastHour(
          Bucketize(all_clicks_last_hour_->GetTotal(duration), kClickBuckets))
      .SetAllClicksLast24Hours(Bucketize(
          all_clicks_last_24_hours_->GetTotal(duration), kClickBuckets))
      .SetTotalHours(ExponentialBucket(duration.InHours(),
                                       kTotalHoursBucketSizeMultiplier))
      .Record(ukm::UkmRecorder::Get());

  // Log click data about the app clicked on and up to five other apps. This
  // represents the state of the data immediately before the click.
  const std::vector<std::string> apps_to_log =
      ChooseAppsToLog(app_launch_event.app_id());

  for (std::string app_id : apps_to_log) {
    auto app = app_features_map_.find(app_id);
    if (app == app_features_map_.end()) {
      continue;
    }
    ukm::SourceId click_data_source_id =
        GetSourceId(app->second.app_type(), app->first,
                    app->second.arc_package_name(), app->second.pwa_url());
    if (click_data_source_id == ukm::kInvalidSourceId) {
      continue;
    }
    ukm::builders::AppListAppClickData app_click_data(click_data_source_id);
    if (!app->second.has_most_recently_used_index()) {
      // This app has not been clicked on this session, so log fewer metrics.
      app_click_data.SetAppType(app->second.app_type())
          .SetAppLaunchId(launch_source_id)
          .Record(ukm::UkmRecorder::Get());
      continue;
    }
    app->second.set_time_since_last_click_sec(
        now.ToDeltaSinceWindowsEpoch().InSeconds() -
        app->second.time_of_last_click_sec());

    LogClicksEachHour(app->second, &app_click_data);

    app_click_data.SetAppType(app->second.app_type())
        .SetAppLaunchId(launch_source_id)
        .SetMostRecentlyUsedIndex(app->second.most_recently_used_index())
        .SetTimeSinceLastClick(
            Bucketize(app->second.time_since_last_click_sec(),
                      kTimeSinceLastClickBuckets))
        .SetClicksLastHour(
            Bucketize(app->second.clicks_last_hour(), kClickBuckets))
        .SetClicksLast24Hours(
            Bucketize(app->second.clicks_last_24_hours(), kClickBuckets))
        .SetTotalClicks(Bucketize(app->second.total_clicks(), kClickBuckets))
        .SetLastLaunchedFrom(app->second.last_launched_from())
        .Record(ukm::UkmRecorder::Get());
  }
  ProcessClick(app_launch_event, now);
}

}  // namespace app_list
