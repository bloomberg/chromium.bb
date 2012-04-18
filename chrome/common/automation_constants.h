// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOMATION_CONSTANTS_H__
#define CHROME_COMMON_AUTOMATION_CONSTANTS_H__
#pragma once

#include <string>

namespace automation {

// JSON value labels for proxy settings that are passed in via
// AutomationMsg_SetProxyConfig. These are here since they are used by both
// AutomationProvider and AutomationProxy.
extern const char kJSONProxyAutoconfig[];
extern const char kJSONProxyNoProxy[];
extern const char kJSONProxyPacUrl[];
extern const char kJSONProxyPacMandatory[];
extern const char kJSONProxyBypassList[];
extern const char kJSONProxyServer[];

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

// The current version of ChromeDriver automation supported by Chrome.
// This needs to be incremented for each change to ChromeDriver automation that
// is not backwards compatible. Some examples of this would be:
// - SendJSONRequest or Hello IPC messages change
// - The interface for an individual ChromeDriver automation call changes in an
//   incompatible way
// TODO(kkania): Investigate a better backwards compatible automation solution.
extern const int kChromeDriverAutomationVersion;

// Automation error codes. These provide the client a simple way
// to detect certain types of errors it may be interested in handling.
// The error code values must stay consistent across compatible versions.
enum ErrorCode {
  // An unknown error occurred.
  kUnknownError = 0,
  // Trying to operate on a JavaScript modal dialog when none is open.
  kNoJavaScriptModalDialogOpen = 1,
  // An open modal dialog blocked the operation. The operation may have
  // partially completed.
  kBlockedByModalDialog = 2,
  // An ID was supplied that is invalid or does not refer to an existing object.
  kInvalidId = 3,
};

// Represents an automation error. Each error has a code and an error message.
class Error {
 public:
  // Creates an invalid error.
  Error();

  // Creates an error for the given code. A default message for the given code
  // will be used as the error message.
  explicit Error(ErrorCode code);

  // Creates an error for the given message. The |kUnknownError| type will
  // be used.
  explicit Error(const std::string& error_msg);

  // Creates an error for the given code and message.
  Error(ErrorCode code, const std::string& error_msg);

  virtual ~Error();

  ErrorCode code() const;
  const std::string& message() const;

 private:
  ErrorCode code_;
  std::string message_;
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
