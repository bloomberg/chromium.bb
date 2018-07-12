// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_TEST_INPUT_H_
#define CHROME_BROWSER_VR_UI_TEST_INPUT_H_

#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

// These are used to map user-friendly names, e.g. URL_BAR, to the underlying
// element names for interaction during testing.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class UserFriendlyElementName : int {
  kUrl = 0,         // URL bar
  kBackButton,      // Back button on the URL bar
  kForwardButton,   // Forward button in the overflow menu
  kReloadButton,    // Reload button in the overflow menu
  kOverflowMenu,    // Overflow menu
  kPageInfoButton,  // Page info button on the URL bar
  kBrowsingDialog,  // 2D fallback UI, e.g. permission prompts
  kContentQuad,     // Main content quad showing web contents
};

// These are used to report the current state of the UI after performing an
// action
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class VrUiTestActivityResult : int {
  kUnreported,
  kQuiescent,
  kTimeoutNoStart,
  kTimeoutNoEnd,
};

// These are used to specify what type of action should be performed on a UI
// element using simulated controller input during testing.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class VrControllerTestAction : int {
  kClick,
  kHover,
  kRevertToRealController,
};

// Holds all information necessary to perform a simulated controller action on
// a UI element.
struct ControllerTestInput {
  UserFriendlyElementName element_name;
  VrControllerTestAction action;
  gfx::PointF position;
};

struct UiTestActivityExpectation {
  int quiescence_timeout_ms;
};

// Holds all the information necessary to keep track of and report whether the
// UI reacted to test input.
struct UiTestState {
  // Whether the UI has started updating/reacting since we started tracking
  bool activity_started = false;
  // The number of frames to wait for the UI to stop having activity before
  // timing out.
  base::TimeDelta quiescence_timeout_ms = base::TimeDelta::Min();
  // The total number of frames that have been rendered since tracking started.
  base::TimeTicks start_time = base::TimeTicks::Now();
};

UiElementName UserFriendlyElementNameToUiElementName(
    UserFriendlyElementName name);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_TEST_INPUT_H_
