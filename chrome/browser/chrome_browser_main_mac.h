// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_

#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_posix.h"
#include "chrome/browser/resource_delegate_mac.h"

class ChromeBrowserMainPartsMac : public ChromeBrowserMainPartsPosix {
 public:
  explicit ChromeBrowserMainPartsMac(
      const content::MainFunctionParams& parameters);
  ~ChromeBrowserMainPartsMac() override;

  // BrowserParts overrides.
  void PreEarlyInitialization() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  void PreProfileInit() override;
  void PostProfileInit() override;

  // Perform platform-specific work that needs to be done after the main event
  // loop has ended. The embedder must be sure to call this.
  static void DidEndMainMessageLoop();

 private:
  MacResourceDelegate resource_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsMac);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
