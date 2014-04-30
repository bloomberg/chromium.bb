// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

class LocationWatchLocationFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("location.watchLocation",
                             LOCATION_WATCHLOCATION)

 protected:
  virtual ~LocationWatchLocationFunction() {}

  // SyncExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class LocationClearWatchFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("location.clearWatch",
                             LOCATION_CLEARWATCH)

 protected:
  virtual ~LocationClearWatchFunction() {}

  // SyncExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_API_H_
