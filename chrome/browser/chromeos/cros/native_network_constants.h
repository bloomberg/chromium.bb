// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NATIVE_NETWORK_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NATIVE_NETWORK_CONSTANTS_H_

#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

// Format of the Carrier ID.
extern const char kCarrierIdFormat[];

// Path of the default (shared) shill profile.
extern const char kSharedProfilePath[];

extern const char* ConnectionTypeToString(ConnectionType type);
extern const char* SecurityToString(ConnectionSecurity security);
extern const char* ProviderTypeToString(ProviderType type);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NATIVE_NETWORK_CONSTANTS_H_
