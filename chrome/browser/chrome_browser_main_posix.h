// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_CHROME_MAIN_POSIX_H_
#define CHROME_BROWSER_BROWSER_CHROME_MAIN_POSIX_H_

#include "chrome/browser/chrome_browser_main.h"

class ChromeBrowserMainPartsPosix : public ChromeBrowserMainParts {
 public:
  explicit ChromeBrowserMainPartsPosix(const MainFunctionParams& parameters);

  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
};

#endif  // CHROME_BROWSER_BROWSER_CHROME_MAIN_POSIX_H_
