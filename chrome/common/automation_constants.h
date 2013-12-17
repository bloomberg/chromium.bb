// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOMATION_CONSTANTS_H__
#define CHROME_COMMON_AUTOMATION_CONSTANTS_H__

#include <string>

namespace automation {

// When passing the kTestingChannelID switch to the browser, prepend
// this prefix to the channel id to enable the named testing interface.
// Named testing interface is used when you want to connect an
// AutomationProxy to an already-running browser instance.
extern const char kNamedInterfacePrefix[];

// Amount of time to wait before querying the browser.
static const int kSleepTime = 250;

// Recognized by the AutomationProvider's SendWebKeyboardEventToSelectedTab
// command. Specifies the type of the keyboard event.
enum KeyEventTypes {
  kRawKeyDownType = 0,
  kKeyDownType,
  kCharType,
  kKeyUpType,
};

// Recognized by the AutomationProvider's SendWebKeyboardEventToSelectedTab
// command. Specifies masks to be used in constructing keyboard event modifiers.
enum KeyModifierMasks {
  kShiftKeyMask   = 1 << 0,
  kControlKeyMask = 1 << 1,
  kAltKeyMask     = 1 << 2,
  kMetaKeyMask    = 1 << 3,
  kNumLockKeyMask = 1 << 4,
};

// Recognized by the AutomationProvider's ProcessWebMouseEvent command.
enum MouseEventType {
  kMouseDown = 0,
  kMouseUp,
  kMouseMove,
  kMouseEnter,
  kMouseLeave,
  kContextMenu,
};

enum MouseButton {
  kLeftButton = 0,
  kMiddleButton,
  kRightButton,
  kNoButton,
};

}  // namespace automation

// Used by AutomationProxy, declared here so that other headers don't need
// to include automation_proxy.h.
enum AutomationLaunchResult {
  AUTOMATION_LAUNCH_RESULT_INVALID = -1,
  AUTOMATION_SUCCESS,
  AUTOMATION_TIMEOUT,
  AUTOMATION_VERSION_MISMATCH,
  AUTOMATION_CREATE_TAB_FAILED,
  AUTOMATION_SERVER_CRASHED,
  AUTOMATION_CHANNEL_ERROR,
};

enum AutomationMsg_NavigationResponseValues {
  AUTOMATION_MSG_NAVIGATION_ERROR = 0,
  AUTOMATION_MSG_NAVIGATION_SUCCESS,
  AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
  AUTOMATION_MSG_NAVIGATION_BLOCKED_BY_MODAL_DIALOG,
};

// Used in the AutomationMsg_GetExtensionProperty to identify which extension
// property should be retrieved, instead of having separate messages for each
// property.
enum AutomationMsg_DEPRECATED_ExtensionProperty {
  AUTOMATION_MSG_EXTENSION_ID = 0,
  AUTOMATION_MSG_EXTENSION_NAME,
  AUTOMATION_MSG_EXTENSION_VERSION,
  AUTOMATION_MSG_EXTENSION_BROWSER_ACTION_INDEX,
};

// Specifies the font size on a page which is requested by an automation
// client.
enum AutomationPageFontSize {
  SMALLEST_FONT = 8,
  SMALL_FONT = 12,
  MEDIUM_FONT = 16,
  LARGE_FONT = 24,
  LARGEST_FONT = 36
};

enum FindInPageDirection { BACK = 0, FWD = 1 };
enum FindInPageCase { IGNORE_CASE = 0, CASE_SENSITIVE = 1 };

#endif  // CHROME_COMMON_AUTOMATION_CONSTANTS_H__
