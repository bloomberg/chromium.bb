// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCATION_BAR_UTIL_H_
#define CHROME_BROWSER_LOCATION_BAR_UTIL_H_

#include <string>

class Profile;

namespace location_bar_util {

// Returns the short name for a keyword.
std::wstring GetKeywordName(Profile* profile, const std::wstring& keyword);

// Build a short string to use in keyword-search when the field isn't
// very big.
std::wstring CalculateMinString(const std::wstring& description);

}  // namespace location_bar_util

#endif  // CHROME_BROWSER_LOCATION_BAR_UTIL_H_
