// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UTIL_CHROMEOS_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_UTIL_CHROMEOS_H_

#include <string>

#include "base/callback_forward.h"

namespace google_util {
namespace chromeos {

// Returns the brand code stored in Local State that has been assigned to a
// partner. Returns empty string if the information is not available.
std::string GetBrand();

// Clears brand code for the current session (not persisted through browser
// restart). Future calls to GetBrand() will return an empty string.
void ClearBrandForCurrentSession();

// Reads the brand code from a board-specific file and stores it to Local State.
// |callback| is invoked on the calling thread after that.
void SetBrandFromFile(const base::Closure& callback);

}  // namespace chromeos
}  // namespace google_util

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UTIL_CHROMEOS_H_
