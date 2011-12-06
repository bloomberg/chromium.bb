// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This extension API contains system-wide preferences and functions that shall
// be only available to component extensions.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_SYSTEM_API_H_

#include "chrome/browser/extensions/extension_preference_api.h"

#include <string>

namespace base {
class Value;
}

namespace extensions {

class GetIncognitoModeAvailabilityFunction : public SyncExtensionFunction {
 public:
  virtual ~GetIncognitoModeAvailabilityFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("systemPrivate.getIncognitoModeAvailability")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_SYSTEM_API_H_
