// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_DEMO_SESSION_METRICS_RECORDER_H_
#define ASH_METRICS_DEMO_SESSION_METRICS_RECORDER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace base {
class RepeatingTimer;
}  // namespace base

namespace ui {
class UserActivityDetector;
}  // namespace ui

namespace ash {

// A metrics recorder for demo sessions that samples the active window's app or
// window type. Only used when the device is in Demo Mode.
class ASH_EXPORT DemoSessionMetricsRecorder
    : public ui::UserActivityObserver,
      public wm::ActivationChangeObserver {
 public:
  // These apps are preinstalled in Demo Mode. This list is not exhaustive, and
  // includes first- and third-party Chrome and ARC apps.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DemoModeApp {
    kBrowser = 0,
    kOtherChromeApp = 1,
    kOtherArcApp = 2,
    kOtherWindow = 3,
    kHighlights = 4,  // Auto-launched Demo Mode app highlighting CrOS features.
    kAsphalt8 = 5,    // Android racing game demo app.
    kCamera = 6,
    kFiles = 7,
    kGetHelp = 8,
    kGoogleKeep = 9,
    kGooglePhotos = 10,
    kGoogleSheets = 11,
    kGoogleSlides = 12,
    kInfinitePainter = 13,  // Android painting app.
    kMyScriptNebo = 14,     // Android note-taking app.
    kPlayStore = 15,
    kSquid = 16,  // Android note-taking app.
    kWebStore = 17,
    kYouTube = 18,
    // Add future entries above this comment, in sync with enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kYouTube,
  };

  // The recorder will create a normal timer by default. Tests should provide a
  // mock timer to control sampling periods.
  explicit DemoSessionMetricsRecorder(
      std::unique_ptr<base::RepeatingTimer> timer = nullptr);
  ~DemoSessionMetricsRecorder() override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  // Starts the timer for periodic sampling.
  void StartRecording();

  // Records the active window's app type or, if the user has been inactive for
  // too long, pauses sampling and wipes samples from the inactive period.
  void TakeSampleOrPause();

  // Emits histograms for recorded samples.
  void ReportSamples();

  // Emits histograms for the number of unique apps launched.
  void ReportUniqueAppsLaunched();

  // Stores samples as they are collected. Report to UMA if we see user
  // activity soon after. Guaranteed not to grow too large.
  std::vector<DemoModeApp> unreported_samples_;

  // Tracks the ids of apps that have been launched in Demo Mode.
  base::flat_set<std::string> unique_apps_launched_;

  // How many periods have elapsed since the last user activity.
  int periods_since_activity_ = 0;

  std::unique_ptr<base::RepeatingTimer> timer_;

  ScopedObserver<ui::UserActivityDetector, DemoSessionMetricsRecorder>
      observer_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionMetricsRecorder);
};

}  // namespace ash

#endif  // ASH_METRICS_DEMO_SESSION_METRICS_RECORDER_H_
