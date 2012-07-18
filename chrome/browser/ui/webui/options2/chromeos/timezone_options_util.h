// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_TIMEZONE_OPTIONS_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_TIMEZONE_OPTIONS_UTIL_H_

#include "base/memory/scoped_ptr.h"

namespace base {
class ListValue;
}

namespace options2 {

// Creates a list of pairs of each timezone's ID and name.
scoped_ptr<base::ListValue> GetTimezoneList();

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_TIMEZONE_OPTIONS_UTIL_H_
