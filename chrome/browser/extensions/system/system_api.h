// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This extension API contains system-wide preferences and functions that shall
// be only available to component extensions.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_SYSTEM_API_H_

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class GetIncognitoModeAvailabilityFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("systemPrivate.getIncognitoModeAvailability",
                             SYSTEMPRIVATE_GETINCOGNITOMODEAVAILABILITY)

 protected:
  virtual ~GetIncognitoModeAvailabilityFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// API function which returns the status of system update.
class GetUpdateStatusFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("systemPrivate.getUpdateStatus",
                             SYSTEMPRIVATE_GETUPDATESTATUS)

 protected:
  virtual ~GetUpdateStatusFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// Dispatches systemPrivate.onBrightnessChanged event for extensions.
void DispatchBrightnessChangedEvent(int brightness, bool user_initiated);

// Dispatches systemPrivate.onVolumeChanged event for extensions.
void DispatchVolumeChangedEvent(double volume, bool is_volume_muted);

// Dispatches systemPrivate.onScreenChanged event for extensions.
void DispatchScreenUnlockedEvent();

// Dispatches systemPrivate.onWokeUp event for extensions.
void DispatchWokeUpEvent();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_SYSTEM_API_H_
