// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/version.h"

#include "base/strings/stringprintf.h"

namespace {

// This variable must be able to be found and parsed by the upload script.
const int kMinimumSupportedChromeVersion[] = {35, 0, 1916, 0};

}  // namespace

const int kMinimumSupportedChromeBuildNo = kMinimumSupportedChromeVersion[2];

// Content Shell and WebView have an empty product version and a fake user
// agent. There's no way to detect the actual version, so unless specified we
// assume it is tip of tree.
static const int kToTBuildNo = 9999;

// Similarly, if the Blink Revision isn't given then assume it is tip of tree.
static const int kToTBlinkRevision = 999999;

BrowserInfo::BrowserInfo()
    : browser_name(std::string()),
      browser_version(std::string()),
      build_no(kToTBuildNo),
      blink_revision(kToTBlinkRevision) {
}

BrowserInfo::BrowserInfo(std::string browser_name,
                         std::string browser_version,
                         int build_no,
                         int blink_revision)
    : browser_name(browser_name),
      browser_version(browser_version),
      build_no(build_no),
      blink_revision(blink_revision) {
}

std::string GetMinimumSupportedChromeVersion() {
  return base::StringPrintf(
      "%d.%d.%d.%d",
      kMinimumSupportedChromeVersion[0],
      kMinimumSupportedChromeVersion[1],
      kMinimumSupportedChromeVersion[2],
      kMinimumSupportedChromeVersion[3]);
}
