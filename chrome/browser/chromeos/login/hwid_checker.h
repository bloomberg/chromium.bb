// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HWID_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HWID_CHECKER_H_

#include <string>

namespace chromeos {

// Calculates checksum for given string as it made for HWID v1 and v2.
std::string CalculateHWIDChecksum(const std::string& data);

// Checks if given HWID correct in terms of HWID v1 or v2.
bool IsHWIDCorrect(const std::string& hwid);

// Checks if current machine has correct HWID v1 or v2.
bool IsMachineHWIDCorrect();

} // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HWID_CHECKER_H_

