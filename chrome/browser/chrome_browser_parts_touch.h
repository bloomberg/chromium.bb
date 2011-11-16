// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_PARTS_TOUCH_H_
#define CHROME_BROWSER_CHROME_BROWSER_PARTS_TOUCH_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/browser_main_parts.h"

class ChromeBrowserPartsTouch : public content::BrowserMainParts {
 public:
  ChromeBrowserPartsTouch();

  virtual void PreEarlyInitialization() OVERRIDE {}
  virtual void PostEarlyInitialization() OVERRIDE {}
  virtual void PreMainMessageLoopStart() OVERRIDE {}
  virtual void ToolkitInitialized() OVERRIDE {}
  virtual void PostMainMessageLoopStart() OVERRIDE {}
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserPartsTouch);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_PARTS_TOUCH_H_
