// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_launch_event_logger.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/ukm/app_source_url_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace app_list {

const base::Feature kUkmAppLaunchEventLogging{"UkmAppLaunchEventLogging",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
namespace {

const char kExtensionSchemeWithDelimiter[] = "chrome-extension://";

AppLaunchEvent GetAppLaunchEvent(const ChromeSearchResult* result) {
  AppLaunchEvent event;
  event.set_launched_from(AppLaunchEvent_LaunchedFrom_SUGGESTED);
  std::string id = result->id();
  // For chrome apps, result->id() returns "chrome-extension://|id|/" instead
  // of just |id|, so remove any leading "chrome-extension://" and trailing "/".
  if (!id.compare(0, strlen(kExtensionSchemeWithDelimiter),
                  kExtensionSchemeWithDelimiter)) {
    id.erase(0, strlen(kExtensionSchemeWithDelimiter));
  }
  if (id.size() && !id.compare(id.size() - 1, 1, "/")) {
    id.pop_back();
  }
  event.set_app_id(id);
  return event;
}

AppLaunchEvent GetAppLaunchEvent(const std::string& id) {
  AppLaunchEvent event;
  event.set_launched_from(AppLaunchEvent_LaunchedFrom_GRID);
  event.set_app_id(id);
  return event;
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

AppLaunchEventLogger::AppLaunchEventLogger() : weak_factory_(this) {
  CreateTaskRunner();
  PopulateIdAppTypeMap();
  PopulatePwaIdUrlMap();
}

AppLaunchEventLogger::~AppLaunchEventLogger() {}

void AppLaunchEventLogger::CreateTaskRunner() {
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

void AppLaunchEventLogger::OnSuggestionChipClicked(
    const ChromeSearchResult* result) {
  if (base::FeatureList::IsEnabled(kUkmAppLaunchEventLogging)) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AppLaunchEventLogger::OnAppLaunched,
                       weak_factory_.GetWeakPtr(), GetAppLaunchEvent(result)));
  }
}

void AppLaunchEventLogger::OnGridClicked(const std::string& id) {
  if (base::FeatureList::IsEnabled(kUkmAppLaunchEventLogging)) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AppLaunchEventLogger::OnAppLaunched,
                       weak_factory_.GetWeakPtr(), GetAppLaunchEvent(id)));
  }
}

AppLaunchEventLogger& AppLaunchEventLogger::GetInstance() {
  static base::NoDestructor<AppLaunchEventLogger> instance;
  return *instance;
}

void AppLaunchEventLogger::OnAppLaunched(AppLaunchEvent app_launch_event) {
  AppLaunchEvent_AppType app_type(GetAppType(app_launch_event.app_id()));
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListAppTypeClicked", app_type,
                            AppLaunchEvent_AppType_AppType_ARRAYSIZE);

  ukm::SourceId source_id = 0;
  if (app_type == AppLaunchEvent_AppType_CHROME) {
    source_id = ukm::AppSourceUrlRecorder::GetSourceIdForChromeApp(
        app_launch_event.app_id());
  } else if (app_type == AppLaunchEvent_AppType_PWA) {
    source_id = ukm::AppSourceUrlRecorder::GetSourceIdForPWA(
        GURL(GetPwaUrl(app_launch_event.app_id())));
  } else {
    // TODO(pdyson): Handle Play apps.
    return;
  }

  ukm::builders::AppListAppLaunch app_launch(source_id);
  base::Time now(base::Time::Now());
  app_launch.SetAppType(app_type)
      .SetLaunchedFrom(app_launch_event.launched_from())
      .SetDayOfWeek(DayOfWeek(now))
      .SetHourOfDay(HourOfDay(now))
      .Record(ukm::UkmRecorder::Get());
}

void AppLaunchEventLogger::PopulateIdAppTypeMap() {
  id_app_type_map_ = base::flat_map<std::string, AppLaunchEvent_AppType>{
      // These apps are taken from extensions/common/constants.cc. They do not
      // include the extensions in that file, only the apps.
      // Camera
      {"hfhhnacclhffhdffklopdkcgdhifgngh", AppLaunchEvent_AppType_CHROME},
      // Chrome
      {"mgndgikekgjfcpckkfioiadnlibdjbkf", AppLaunchEvent_AppType_CHROME},
      // FilesManager
      {"hhaomjibdihmijegdhdafkllkbggdgoj", AppLaunchEvent_AppType_CHROME},
      // GoogleKeep
      {"hmjkmjkepdijhoojdojkdfohbdgmmhki", AppLaunchEvent_AppType_CHROME},
      // Youtube
      {"blpcfgokakmgnkcojhhkbfbldkacnbeo", AppLaunchEvent_AppType_CHROME},
      // Genius
      {"ljoammodoonkhnehlncldjelhidljdpi", AppLaunchEvent_AppType_CHROME},
      // Highlights
      {"lpmakjfjcconjeehbidjclhdlpjmfjjj", AppLaunchEvent_AppType_CHROME},
      // HighlightsAlt1
      {"iggildboghmjpbjcpmobahnkmoefkike", AppLaunchEvent_AppType_CHROME},
      // HighlightsAlt2
      {"elhbopodaklenjkeihkdhhfaghalllba", AppLaunchEvent_AppType_CHROME},
      // Screensaver
      {"mnoijifedipmbjaoekhadjcijipaijjc", AppLaunchEvent_AppType_CHROME},
      // ScreensaverAlt1
      {"gdobaoeekhiklaljmhladjfdfkigampc", AppLaunchEvent_AppType_CHROME},
      // ScreensaverAlt2
      {"lminefdanffajachfahfpmphfkhahcnj", AppLaunchEvent_AppType_CHROME},

      // These apps are taken from
      // chrome/common/extensions/extension_constants.cc. They do not include
      // the extensions in that file, only the apps.
      // Calculator
      {"joodangkbfjnajiiifokapkpmhfnpleo", AppLaunchEvent_AppType_CHROME},
      // Calendar
      {"ejjicmeblgpmajnghnpcppodonldlgfn", AppLaunchEvent_AppType_CHROME},
      // ChromeRemoteDesktop
      {"gbchcmhmhahfdphkhkmpfmihenigjmpp", AppLaunchEvent_AppType_CHROME},
      // CloudPrint
      {"mfehgcgbbipciphmccgaenjidiccnmng", AppLaunchEvent_AppType_CHROME},
      // DriveHosted
      {"apdfllckaahabafndbhieahigkjlhalf", AppLaunchEvent_AppType_CHROME},
      // EnterpriseWebStore
      {"afchcafgojfnemjkcbhfekplkmjaldaa", AppLaunchEvent_AppType_CHROME},
      // Gmail
      {"pjkljhegncpnkpknbcohdijeoejaedia", AppLaunchEvent_AppType_CHROME},
      // GoogleDoc
      {"aohghmighlieiainnegkcijnfilokake", AppLaunchEvent_AppType_CHROME},
      // GoogleMaps
      {"lneaknkopdijkpnocmklfnjbeapigfbh", AppLaunchEvent_AppType_CHROME},
      // GooglePhotos
      {"hcglmfcclpfgljeaiahehebeoaiicbko", AppLaunchEvent_AppType_CHROME},
      // GooglePlayBooks
      {"mmimngoggfoobjdlefbcabngfnmieonb", AppLaunchEvent_AppType_CHROME},
      // GooglePlayMovies
      {"gdijeikdkaembjbdobgfkoidjkpbmlkd", AppLaunchEvent_AppType_CHROME},
      // GooglePlayMusic
      {"icppfcnhkcmnfdhfhphakoifcfokfdhg", AppLaunchEvent_AppType_CHROME},
      // GooglePlus
      {"dlppkpafhbajpcmmoheippocdidnckmm", AppLaunchEvent_AppType_CHROME},
      // GoogleSheets
      {"felcaaldnbdncclmgdcncolpebgiejap", AppLaunchEvent_AppType_CHROME},
      // GoogleSlides
      {"aapocclcgogkmnckokdopfmhonfmgoek", AppLaunchEvent_AppType_CHROME},
      // HTerm
      {"pnhechapfaindjhompbnflcldabbghjo", AppLaunchEvent_AppType_CHROME},
      // HTermDev
      {"okddffdblfhhnmhodogpojmfkjmhinfp", AppLaunchEvent_AppType_CHROME},
      // IdentityApiUi
      {"ahjaciijnoiaklcomgnblndopackapon", AppLaunchEvent_AppType_CHROME},
      // CroshBuiltin
      {"nkoccljplnhpfnfiajclkommnmllphnl", AppLaunchEvent_AppType_CHROME},
      // TextEditor
      {"mmfbcljfglbokpmkimbfghdkjmjhdgbg", AppLaunchEvent_AppType_CHROME},
      // InAppPaymentsSupport
      {"nmmhkkegccagdldgiimedpiccmgmieda", AppLaunchEvent_AppType_CHROME},

      // Other
      // Google+
      {"dcdbodpaldbchkfinnjphocleggfceip", AppLaunchEvent_AppType_PWA},
      // Photos
      {"ncmjhecbjeaamljdfahankockkkdmedg", AppLaunchEvent_AppType_PWA}};
}

void AppLaunchEventLogger::PopulatePwaIdUrlMap() {
  pwa_id_url_map_ = base::flat_map<std::string, std::string>{
      // Google+
      {"dcdbodpaldbchkfinnjphocleggfceip", "https://plus.google.com/discover"},
      // Photos
      {"ncmjhecbjeaamljdfahankockkkdmedg", "https://photos.google.com/"}};
}

AppLaunchEvent_AppType AppLaunchEventLogger::GetAppType(const std::string& id) {
  auto search = id_app_type_map_.find(id);
  if (search != id_app_type_map_.end()) {
    return search->second;
  }
  return AppLaunchEvent_AppType_OTHER;
}

const std::string& AppLaunchEventLogger::GetPwaUrl(const std::string& id) {
  auto search = pwa_id_url_map_.find(id);
  if (search != pwa_id_url_map_.end()) {
    return search->second;
  }
  return base::EmptyString();
}

}  // namespace app_list
