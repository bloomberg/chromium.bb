// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_UTIL_H_
#define CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_UTIL_H_

#include "base/string16.h"

namespace location_bar_util {

// Build a short string to use in keyword-search when the field isn't very big.
string16 CalculateMinString(const string16& description);

}  // namespace location_bar_util

#endif  // CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_UTIL_H_
