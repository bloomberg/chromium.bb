// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/demo_session_metrics_recorder.h"

#include <string>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
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
    // The ShelfID app id isn't used to identify ARC++ apps since it's a hash of
    // both the package name and the activity.
    const std::string* package_name =
        static_cast<std::string*>(window->GetProperty(kArcPackageNameKey));
    return package_name ? GetAppFromPackageName(*package_name)
                        : DemoModeApp::kOtherArcApp;
  }

  std::string app_id =
      ShelfID::Deserialize(window->GetProperty(kShelfIDKey)).app_id;

  // The Chrome "app" in the shelf is just the browser.
  if (app_id == extension_misc::kChromeAppId)
    return DemoModeApp::kBrowser;

  // If the window is the "browser" type, having an app ID other than
  // kChromeAppId indicates a hosted/bookmark app.
  if (app_type == ash::AppType::CHROME_APP ||
      (app_type == ash::AppType::BROWSER && app_id.size())) {
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
  observer_.Add(ui::UserActivityDetector::Get());
}

DemoSessionMetricsRecorder::~DemoSessionMetricsRecorder() {
  // Report any remaining stored samples on exit. (If the user went idle, there
  // won't be any.)
  ReportSamples();
}

void DemoSessionMetricsRecorder::OnUserActivity(const ui::Event* event) {
  // Report samples recorded since the last activity.
  ReportSamples();

  // Restart the timer if the device has been idle.
  if (!timer_->IsRunning())
    StartRecording();
  periods_since_activity_ = 0;
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

}  // namespace ash
