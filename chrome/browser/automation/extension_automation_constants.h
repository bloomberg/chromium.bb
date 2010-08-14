// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to encode requests and responses for automation.

#ifndef CHROME_BROWSER_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_
#define CHROME_BROWSER_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_
#pragma once

namespace extension_automation_constants {

// All extension automation related messages will have this origin.
extern const char kAutomationOrigin[];
// Key used for all extension automation request types.
extern const char kAutomationRequestIdKey[];

// Keys used for API communications
extern const char kAutomationHasCallbackKey[];
extern const char kAutomationErrorKey[];  // not present implies success
extern const char kAutomationNameKey[];
extern const char kAutomationArgsKey[];
extern const char kAutomationResponseKey[];
// All external API requests have this target.
extern const char kAutomationRequestTarget[];
// All API responses should have this target.
extern const char kAutomationResponseTarget[];

// Keys used for port communications
extern const char kAutomationConnectionIdKey[];
extern const char kAutomationMessageDataKey[];
extern const char kAutomationExtensionIdKey[];
extern const char kAutomationPortIdKey[];
extern const char kAutomationChannelNameKey[];
extern const char kAutomationTabJsonKey[];

// All external port message requests should have this target.
extern const char kAutomationPortRequestTarget[];
// All external port message responses have this target.
extern const char kAutomationPortResponseTarget[];

// All external browser events have this target.
extern const char kAutomationBrowserEventRequestTarget[];

// The command codes for our private port protocol.
enum PrivatePortCommand {
  OPEN_CHANNEL = 0,
  CHANNEL_OPENED = 1,
  POST_MESSAGE = 2,
  CHANNEL_CLOSED = 3,
};

};  // namespace automation_extension_constants

#endif  // CHROME_BROWSER_AUTOMATION_EXTENSION_AUTOMATION_CONSTANTS_H_
