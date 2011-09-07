// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_MAIN_MAC_H_
#define CHROME_BROWSER_BROWSER_MAIN_MAC_H_
#pragma once

#include "chrome/browser/browser_main_posix.h"

class BrowserMainPartsMac : public BrowserMainPartsPosix {
 public:
  explicit BrowserMainPartsMac(const MainFunctionParams& parameters);

  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void InitializeSSL() OVERRIDE;
};

#endif  // CHROME_BROWSER_BROWSER_MAIN_WIN_H_
