// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UTIL_H_
#define CHROME_UPDATER_WIN_UTIL_H_

#include "base/win/windows_types.h"

namespace updater {

// Returns the last error as an HRESULT or E_FAIL if last error is NO_ERROR.
// This is not a drop in replacement for the HRESULT_FROM_WIN32 macro.
// The macro maps a NO_ERROR to S_OK, whereas the HRESULTFromLastError maps a
// NO_ERROR to E_FAIL.
HRESULT HRESULTFromLastError();

// Returns an HRESULT with a custom facility code representing an updater error.
template <typename Error>
HRESULT HRESULTFromUpdaterError(Error error) {
  constexpr ULONG kCustomerBit = 0x20000000;
  constexpr ULONG kFacilityOmaha = 67;
  return static_cast<HRESULT>(static_cast<ULONG>(SEVERITY_ERROR) |
                              kCustomerBit | (kFacilityOmaha << 16) |
                              static_cast<ULONG>(error));
}

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UTIL_H_
