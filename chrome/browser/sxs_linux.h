// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SXS_LINUX_H_
#define CHROME_BROWSER_SXS_LINUX_H_

#include "base/compiler_specific.h"

namespace base {
class FilePath;
}

namespace sxs_linux {

// Records channel of the running browser in user data dir. This is needed
// to support a seamless and automatic upgrade from non-side-by-side
// to side-by-side Linux packages (the latter use different default data dirs).
// Must be run on FILE thread.
void AddChannelMarkToUserDataDir();

}  // namespace sxs_linux

#endif  // CHROME_BROWSER_SXS_LINUX_H_
