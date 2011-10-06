// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are gtk-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_GTK_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chrome_browser_main_posix.h"
#include "chrome/browser/chrome_browser_main_x11.h"

class ChromeBrowserMainPartsGtk : public ChromeBrowserMainPartsPosix {
 public:
  explicit ChromeBrowserMainPartsGtk(const MainFunctionParams& parameters);

  virtual void PreEarlyInitialization() OVERRIDE;

 private:
  void DetectRunningAsRoot();
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_GTK_H_
