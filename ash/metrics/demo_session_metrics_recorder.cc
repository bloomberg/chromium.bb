// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/demo_session_metrics_recorder.h"

#include <string>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

using DemoModeApp = DemoSessionMetricsRecorder::DemoModeApp;

// How often to sample.
constexpr auto kSamplePeriod = base::TimeDelta::FromSeconds(1);

// How many periods to wait for user activity before discarding samples.
// This timeout is low because demo sessions tend to be very short. If we
// recorded samples for a full minute while the device is in between uses, we
// would bias our measurements toward whatever app was used last.
constexpr int kMaxPeriodsWithoutActivity =
    base::TimeDelta::FromSeconds(15) / kSamplePeriod;

// Returns a package name for an ARC window. The returned value is unowned and
// may be null.
const std::string* GetPackageNameForArcWindow(const aura::Window* window) {
  // Make sure this is an ARC app window.
  DCHECK(static_cast<ash::AppType>(window->GetProperty(
             aura::client::kAppType)) == ash::AppType::ARC_APP);
  // We use a dedicated key for instead of kShelfIDKey for identifiying ARC++
  // apps. The ShelfID app id isn't used to identify ARC++ apps since it's a
  // hash of both the package name and the activity.
  return static_cast<std::string*>(window->GetProperty(kArcPackageNameKey));
}

// Returns the app ID For a non-ARC window. This function crashes if the
// window is an ARC window.
std::string GetAppIdForWindow(const aura::Window* window) {
  DCHECK(static_cast<ash::AppType>(window->GetProperty(
             aura::client::kAppType)) != ash::AppType::ARC_APP);
  return ShelfID::Deserialize(window->GetProperty(kShelfIDKey)).app_id;
}

// Maps a Chrome app ID to a DemoModeApp value for metrics.
DemoModeApp GetAppFromAppId(const std::string& app_id) {
  // Each version of the Highlights app is bucketed into the same value.
  if (app_id == extension_misc::kHighlightsAppId ||
      app_id == extension_misc::kHighlightsAlt1AppId ||
      app_id == extension_misc::kHighlightsAlt2AppId) {
    return DemoModeApp::kHighlights;
  }

  if (app_id == extension_misc::kCameraAppId)
    return DemoModeApp::kCamera;
  if (app_id == extension_misc::kFilesManagerAppId)
    return DemoModeApp::kFiles;
  if (app_id == extension_misc::kGeniusAppId)
    return DemoModeApp::kGetHelp;
  if (app_id == extension_misc::kGoogleKeepAppId)
    return DemoModeApp::kGoogleKeep;
  if (app_id == extensions::kWebStoreAppId)
    return DemoModeApp::kWebStore;
  if (app_id == extension_misc::kYoutubeAppId)
    return DemoModeApp::kYouTube;
  return DemoModeApp::kOtherChromeApp;
}

// Maps an ARC++ package name to a DemoModeApp value for metrics.
DemoModeApp GetAppFromPackageName(const std::string& package_name) {
  // Google apps.
  if (package_name == "com.google.Photos")
    return DemoModeApp::kGooglePhotos;
  if (package_name == "com.google.Sheets")
    return DemoModeApp::kGoogleSheets;
  if (package_name == "com.google.Slides")
    return DemoModeApp::kGoogleSlides;
  if (package_name == "com.android.vending")
    return DemoModeApp::kPlayStore;

  // Third-party apps.
  if (package_name == "com.gameloft.android.ANMP.GloftA8HMD")
    return DemoModeApp::kAsphalt8;
  if (package_name == "com.brakefield.painter")
    return DemoModeApp::kInfinitePainter;
  if (package_name == "com.myscript.nebo.demo")
    return DemoModeApp::kMyScriptNebo;
  if (package_name == "com.steadfastinnovation.android.projectpapyrus")
    return DemoModeApp::kSquid;

  return DemoModeApp::kOtherArcApp;
}

// Maps the app-like thing in |window| to a DemoModeApp value for metrics.
DemoModeApp GetAppFromWindow(const aura::Window* window) {
  ash::AppType app_type =
      static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
  if (app_type == ash::AppType::ARC_APP) {
    const std::string* package_name = GetPackageNameForArcWindow(window);
    return package_name ? GetAppFromPackageName(*package_name)
                        : DemoModeApp::kOtherArcApp;
  }

  std::string app_id = GetAppIdForWindow(window);
  // The Chrome "app" in the shelf is just the browser.
  if (app_id == extension_misc::kChromeAppId)
    return DemoModeApp::kBrowser;

  auto is_default = [](const std::string& app_id) {
    if (!features::IsUsingWindowService())
      return app_id.empty();

    return base::StartsWith(app_id, ShelfWindowWatcher::kDefaultShelfIdPrefix,
                            base::CompareCase::SENSITIVE);
  };

  // If the window is the "browser" type, having an app ID other than the
  // default indicates a hosted/bookmark app.
  if (app_type == ash::AppType::CHROME_APP ||
      (app_type == ash::AppType::BROWSER && !is_default(app_id))) {
    return GetAppFromAppId(app_id);
  }

  if (app_type == ash::AppType::BROWSER)
    return DemoModeApp::kBrowser;
  return DemoModeApp::kOtherWindow;
}

}  // namespace

DemoSessionMetricsRecorder::DemoSessionMetricsRecorder(
    std::unique_ptr<base::RepeatingTimer> timer)
    : timer_(std::move(timer)), observer_(this) {
  // Outside of tests, use a normal repeating timer.
  if (!timer_.get())
    timer_ = std::make_unique<base::RepeatingTimer>();

  StartRecording();

  // Listen for user activity events.
  observer_.Add(ui::UserActivityDetector::Get());

  // Listen for Window activation events.
  Shell::Get()->activation_client()->AddObserver(this);
}

DemoSessionMetricsRecorder::~DemoSessionMetricsRecorder() {
  // Report any remaining stored samples on exit. (If the user went idle, there
  // won't be any.)
  ReportSamples();

  // Stop listening for window activation events.
  Shell::Get()->activation_client()->RemoveObserver(this);

  // Report the number of unique apps launched since the last
  // time we reported, and when the demo session ends.  Only
  // do this if there are any entries in the set, because an idle
  // session that was shut down can result in erroneous
  // sample stating that 0 unique apps were launched.
  if (unique_apps_launched_.size() > 0)
    ReportUniqueAppsLaunched();
}

void DemoSessionMetricsRecorder::OnUserActivity(const ui::Event* event) {
  // Report samples recorded since the last activity.
  ReportSamples();

  // Restart the timer if the device has been idle.
  if (!timer_->IsRunning())
    StartRecording();
  periods_since_activity_ = 0;
}

void DemoSessionMetricsRecorder::OnWindowActivated(ActivationReason reason,
                                                   aura::Window* gained_active,
                                                   aura::Window* lost_active) {
  if (gained_active == nullptr)
    return;

  // Don't count popup windows.
  if (gained_active->type() != aura::client::WINDOW_TYPE_NORMAL)
    return;

  // Track unique apps opened.  There is no namespace collision between
  // ARC apps and ChromeOS Apps because ARC app package names use a different
  // naming scheme than ChromeOS Apps.
  std::string unique_app_id;
  ash::AppType app_type = static_cast<ash::AppType>(
      gained_active->GetProperty(aura::client::kAppType));

  if (app_type == ash::AppType::ARC_APP) {
    const std::string* package_name = GetPackageNameForArcWindow(gained_active);
    unique_app_id = *package_name;
  } else {
    unique_app_id = GetAppIdForWindow(gained_active);
  }

  unique_apps_launched_.insert(unique_app_id);
}

void DemoSessionMetricsRecorder::StartRecording() {
  timer_->Start(FROM_HERE, kSamplePeriod, this,
                &DemoSessionMetricsRecorder::TakeSampleOrPause);
}

void DemoSessionMetricsRecorder::TakeSampleOrPause() {
  // After enough inactive time, assume the user left.
  if (++periods_since_activity_ > kMaxPeriodsWithoutActivity) {
    // These samples were collected since the last user activity.
    unreported_samples_.clear();
    timer_->Stop();

    // Since we are assuming that the user left, report how many
    // unique apps have been launched since we last reported.
    ReportUniqueAppsLaunched();
    return;
  }

  const aura::Window* window =
      ash::Shell::Get()->activation_client()->GetActiveWindow();
  if (!window)
    return;

  DemoModeApp app = window->type() == aura::client::WINDOW_TYPE_NORMAL
                        ? GetAppFromWindow(window)
                        : DemoModeApp::kOtherWindow;
  unreported_samples_.push_back(app);
}

void DemoSessionMetricsRecorder::ReportSamples() {
  for (DemoModeApp app : unreported_samples_)
    UMA_HISTOGRAM_ENUMERATION("DemoMode.ActiveApp", app);
  unreported_samples_.clear();
}

void DemoSessionMetricsRecorder::ReportUniqueAppsLaunched() {
  UMA_HISTOGRAM_COUNTS_100("DemoMode.UniqueAppsLaunched",
                           unique_apps_launched_.size());
  unique_apps_launched_.clear();
}

}  // namespace ash
