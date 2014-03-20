// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEOS_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEOS_UTILS_H_

#include "base/strings/string16.h"

namespace chromeos {

// Returns the name of the Chrome device type (eg. Chromebook, Chromebox).
base::string16 GetChromeDeviceType();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEOS_UTILS_H_
