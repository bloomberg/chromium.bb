// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_

#include "base/strings/string16.h"

namespace base {
class Time;
}

namespace history {

// Returns a localized version of |visit_time| including a relative
// indicator (e.g. today, yesterday).
base::string16 GetRelativeDateLocalized(const base::Time& visit_time);

}  // namespace history

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_
