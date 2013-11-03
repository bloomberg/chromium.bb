// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_

#include "base/memory/scoped_ptr.h"

namespace base {
class ListValue;
}

namespace chromeos {
namespace system {

// Creates a list of pairs of each timezone's ID and name.
scoped_ptr<base::ListValue> GetTimezoneList();

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_
