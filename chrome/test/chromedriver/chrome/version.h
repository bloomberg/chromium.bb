// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_VERSION_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_VERSION_H_

#include <string>

struct BrowserInfo {
  BrowserInfo();
  BrowserInfo(std::string browser_name_,
              std::string browser_version_,
              int build_no_,
              int blink_revision_);

  std::string browser_name;
  std::string browser_version;
  int build_no;
  int blink_revision;
};

extern const int kMinimumSupportedChromeBuildNo;

std::string GetMinimumSupportedChromeVersion();

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_VERSION_H_
