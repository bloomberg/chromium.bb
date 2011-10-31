// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are linux-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chrome_browser_main_posix.h"

class ChromeBrowserMainPartsLinux : public ChromeBrowserMainPartsPosix {
 public:
  explicit ChromeBrowserMainPartsLinux(
      const content::MainFunctionParams& parameters);

  // ChromeBrowserMainParts overrides.
  virtual void ShowMissingLocaleMessageBox() OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
