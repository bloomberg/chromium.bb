// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OBSOLETE_OS_H_
#define CHROME_BROWSER_UI_COCOA_OBSOLETE_OS_H_
#pragma once

#include "base/mac/mac_util.h"
#include "base/string16.h"

namespace chrome {

// Returns true if the OS will either be unsupported by future versions of the
// application in the near future, or if this is the last version of the
// application that supports the OS.
inline bool IsOSObsoleteOrNearlySo() {
  return base::mac::IsOSLeopardOrEarlier();
}

// Returns a localized string informing users that their OS will either soon
// be unsupported by future versions of the application, or that they are
// already using the last version of the application that supports their OS.
// Do not use the returned string unless IsOSObsoleteOrNearlySo() returns
// true.
string16 LocalizedObsoleteOSString();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_OBSOLETE_OS_H_
