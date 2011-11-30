// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"

// Interface class for Parts owned by ChromeBrowserMainParts.
// The default implementation for all methods is empty.

// Most of these map to content::BrowserMainParts methods. This interface is
// separate to allow stages to be further subdivided for Chrome specific
// initialization stages (e.g. browser process init, profile init).

class ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraParts();
  virtual ~ChromeBrowserMainExtraParts();

  // EarlyInitialization methods.
  virtual void PreEarlyInitialization();
  virtual void PostEarlyInitialization();

  // ToolkitInitialized methods.
  virtual void ToolkitInitialized();

  // MainMessageLoopStart methods.
  virtual void PreMainMessageLoopStart();
  virtual void PostMainMessageLoopStart();

  // MainMessageLoopRun methods.
  virtual void PostBrowserProcessInit();
  virtual void PostProfileInitialized();
  virtual void PreMainMessageLoopRun();
  virtual void PostMainMessageLoopRun();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraParts);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_
