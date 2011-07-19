// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_MAIN_POSIX_H_
#define CHROME_BROWSER_BROWSER_MAIN_POSIX_H_

#include "chrome/browser/browser_main.h"

class BrowserMainPartsPosix : public BrowserMainParts {
 public:
  explicit BrowserMainPartsPosix(const MainFunctionParams& parameters)
      : BrowserMainParts(parameters) {}

 protected:
  virtual void PreEarlyInitialization();
  virtual void PostMainMessageLoopStart();

 private:
#if !defined(OS_MACOSX)
  virtual void InitializeSSL() {}
#endif
};

#endif  // CHROME_BROWSER_BROWSER_MAIN_POSIX_H_
