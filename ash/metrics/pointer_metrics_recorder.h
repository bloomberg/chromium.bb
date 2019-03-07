// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ASH_METRICS_POINTER_METRICS_RECORDER_H_
#define UI_ASH_METRICS_POINTER_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace ash {

// Form factor of the down event.
// This enum is used to control a UMA histogram buckets. If you change this
// enum, you should update DownEventMetric as well.
enum class DownEventFormFactor {
  kClamshell = 0,
  kTabletModeLandscape,
  kTabletModePortrait,
  kFormFactorCount,
};

// Input type of the down event.
// This enum is used to control a UMA histogram buckets. If you change this
// enum, you should update DownEventMetric as well.
enum class DownEventSource {
  kUnknown = 0,  // Deprecated, never occurs in practice.
  kMouse,
  kStylus,
  kTouch,
  kSourceCount,
};

// App type (Destination), Input and FormFactor Combination of the down event.
// This enum is used to back an UMA histogram and new values should
// be inserted immediately above kCombinationCount.
enum class DownEventMetric2 {
  // All "Unknown" types are deprecated, never occur in practice.
  kOthersUnknownClamshell = 0,
  kOthersUnknownTabletLandscape = 1,
  kOthersUnknownTabletPortrait = 2,
  kOthersMouseClamshell = 3,
  kOthersMouseTabletLandscape = 4,
  kOthersMouseTabletPortrait = 5,
  kOthersStylusClamshell = 6,
  kOthersStylusTabletLandscape = 7,
  kOthersStylusTabletPortrait = 8,
  kOthersTouchClamshell = 9,
  kOthersTouchTabletLandscape = 10,
  kOthersTouchTabletPortrait = 11,
  kBrowserUnknownClamshell = 12,
  kBrowserUnknownTabletLandscape = 13,
  kBrowserUnknownTabletPortrait = 14,
  kBrowserMouseClamshell = 15,
  kBrowserMouseTabletLandscape = 16,
  kBrowserMouseTabletPortrait = 17,
  kBrowserStylusClamshell = 18,
  kBrowserStylusTabletLandscape = 19,
  kBrowserStylusTabletPortrait = 20,
  kBrowserTouchClamshell = 21,
  kBrowserTouchTabletLandscape = 22,
  kBrowserTouchTabletPortrait = 23,
  kChromeAppUnknownClamshell = 24,
  kChromeAppUnknownTabletLandscape = 25,
  kChromeAppUnknownTabletPortrait = 26,
  kChromeAppMouseClamshell = 27,
  kChromeAppMouseTabletLandscape = 28,
  kChromeAppMouseTabletPortrait = 29,
  kChromeAppStylusClamshell = 30,
  kChromeAppStylusTabletLandscape = 31,
  kChromeAppStylusTabletPortrait = 32,
  kChromeAppTouchClamshell = 33,
  kChromeAppTouchTabletLandscape = 34,
  kChromeAppTouchTabletPortrait = 35,
  kArcAppUnknownClamshell = 36,
  kArcAppUnknownTabletLandscape = 37,
  kArcAppUnknownTabletPortrait = 38,
  kArcAppMouseClamshell = 39,
  kArcAppMouseTabletLandscape = 40,
  kArcAppMouseTabletPortrait = 41,
  kArcAppStylusClamshell = 42,
  kArcAppStylusTabletLandscape = 43,
  kArcAppStylusTabletPortrait = 44,
  kArcAppTouchClamshell = 45,
  kArcAppTouchTabletLandscape = 46,
  kArcAppTouchTabletPortrait = 47,
  kCrostiniAppUnknownClamshell = 48,
  kCrostiniAppUnknownTabletLandscape = 49,
  kCrostiniAppUnknownTabletPortrait = 50,
  kCrostiniAppMouseClamshell = 51,
  kCrostiniAppMouseTabletLandscape = 52,
  kCrostiniAppMouseTabletPortrait = 53,
  kCrostiniAppStylusClamshell = 54,
  kCrostiniAppStylusTabletLandscape = 55,
  kCrostiniAppStylusTabletPortrait = 56,
  kCrostiniAppTouchClamshell = 57,
  kCrostiniAppTouchTabletLandscape = 58,
  kCrostiniAppTouchTabletPortrait = 59,
  kMaxValue = kCrostiniAppTouchTabletPortrait
};

// Input, FormFactor, and Destination Combination of the down event.
// This enum is used to back an UMA histogram and new values should
// be inserted immediately above kCombinationCount.
enum class DownEventMetric {
  // All "Unknown" types are deprecated, never occur in practice.
  kUnknownClamshellOthers = 0,
  kUnknownClamshellBrowser,
  kUnknownClamshellChromeApp,
  kUnknownClamshellArcApp,
  kUnknownTabletLandscapeOthers,
  kUnknownTabletLandscapeBrowser,
  kUnknownTabletLandscapeChromeApp,
  kUnknownTabletLandscapeArcApp,
  kUnknownTabletPortraitOthers,
  kUnknownTabletPortraitBrowser,
  kUnknownTabletPortraitChromeApp,
  kUnknownTabletPortraitArcApp,
  kMouseClamshellOthers,
  kMouseClamshellBrowser,
  kMouseClamshellChromeApp,
  kMouseClamshellArcApp,
  kMouseTabletLandscapeOthers,
  kMouseTabletLandscapeBrowser,
  kMouseTabletLandscapeChromeApp,
  kMouseTabletLandscapeArcApp,
  kMouseTabletPortraitOthers,
  kMouseTabletPortraitBrowser,
  kMouseTabletPortraitChromeApp,
  kMouseTabletPortraitArcApp,
  kStylusClamshellOthers,
  kStylusClamshellBrowser,
  kStylusClamshellChromeApp,
  kStylusClamshellArcApp,
  kStylusTabletLandscapeOthers,
  kStylusTabletLandscapeBrowser,
  kStylusTabletLandscapeChromeApp,
  kStylusTabletLandscapeArcApp,
  kStylusTabletPortraitOthers,
  kStylusTabletPortraitBrowser,
  kStylusTabletPortraitChromeApp,
  kStylusTabletPortraitArcApp,
  kTouchClamshellOthers,
  kTouchClamshellBrowser,
  kTouchClamshellChromeApp,
  kTouchClamshellArcApp,
  kTouchTabletLandscapeOthers,
  kTouchTabletLandscapeBrowser,
  kTouchTabletLandscapeChromeApp,
  kTouchTabletLandscapeArcApp,
  kTouchTabletPortraitOthers,
  kTouchTabletPortraitBrowser,
  kTouchTabletPortraitChromeApp,
  kTouchTabletPortraitArcApp,
  kCombinationCount,
};

// A metrics recorder that records pointer related metrics.
class ASH_EXPORT PointerMetricsRecorder : public ui::EventHandler {
 public:
  PointerMetricsRecorder();
  ~PointerMetricsRecorder() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PointerMetricsRecorder);
};

}  // namespace ash

#endif  // UI_ASH_METRICS_POINTER_METRICS_RECORDER_H_
