// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_TEST_INPUT_H_
#define CHROME_BROWSER_VR_UI_TEST_INPUT_H_

#include "chrome/browser/vr/elements/ui_element_name.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

// These are used to map user-friendly names, e.g. URL_BAR, to the underlying
// element names for interaction during testing.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr_shell
enum class UserFriendlyElementName : int {
  kUrl = 0,         // URL bar
  kBackButton,      // Back button on the URL bar
  kForwardButton,   // Forward button in the overflow menu
  kReloadButton,    // Reload button in the overflow menu
  kOverflowMenu,    // Overflow menu
  kPageInfoButton,  // Page info button on the URL bar
};

// These are used to specify what type of action should be performed on a UI
// element during testing.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr_shell
enum class VrUiTestAction : int {
  kHoverEnter,
  kHoverLeave,
  kMove,
  kButtonDown,
  kButtonUp,
  // Scroll actions currently not supported
};

// Holds all information necessary to perform a simulated UI action.
struct UiTestInput {
  UserFriendlyElementName element_name;
  VrUiTestAction action;
  gfx::PointF position;
};

UiElementName UserFriendlyElementNameToUiElementName(
    UserFriendlyElementName name);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_TEST_INPUT_H_
