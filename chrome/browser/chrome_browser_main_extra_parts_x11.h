// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are gtk-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_X11_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_X11_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeBrowserMainExtraPartsX11 : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsX11();
  virtual ~ChromeBrowserMainExtraPartsX11();

 private:
  // ChromeBrowserMainExtraParts overrides.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsX11);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_X11_H_
