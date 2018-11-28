// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_switches.h"

namespace previews {
namespace switches {

// Ignore decisions made by PreviewsBlackList.
const char kIgnorePreviewsBlacklist[] = "ignore-previews-blacklist";

// Override the Lite Page Preview Host.
const char kLitePageServerPreviewHost[] = "litepage-server-previews-host";

// Ignore the optimization hints blacklist for Lite Page Redirect previews.
const char kIgnoreLitePageRedirectOptimizationBlacklist[] =
    "ignore-litepage-redirect-optimization-blacklist";

// Clears the local Lite Page Redirect blacklist on startup.
const char kClearLitePageRedirectLocalBlacklist[] =
    "clear-litepage-redirect-local-blacklist-on-startup";

}  // namespace switches
}  // namespace previews
