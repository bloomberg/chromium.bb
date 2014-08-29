// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_

#include "base/basictypes.h"

namespace content {
class WebContents;
}

namespace extensions {
namespace extension_action_test_util {

// TODO(devlin): Should we also pull out methods to test browser actions?

// Returns the number of page actions that are visible in the given
// |web_contents|.
size_t GetVisiblePageActionCount(content::WebContents* web_contents);

// Returns the total number of page actions (visible or not) for the given
// |web_contents|.
size_t GetTotalPageActionCount(content::WebContents* web_contents);

}  // namespace extension_action_test_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_
