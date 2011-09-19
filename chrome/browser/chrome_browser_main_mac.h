// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
#pragma once

#include "chrome/browser/chrome_browser_main_posix.h"

class ChromeBrowserMainPartsMac : public ChromeBrowserMainPartsPosix {
 public:
  explicit ChromeBrowserMainPartsMac(const MainFunctionParams& parameters);

  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;

  // Perform platform-specific work that needs to be done after the main event
  // loop has ended. The embedder must be sure to call this.
  static void DidEndMainMessageLoop();
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
