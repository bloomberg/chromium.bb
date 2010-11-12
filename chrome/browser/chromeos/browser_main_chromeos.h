// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_BROWSER_MAIN_CHROMEOS_H_

#include "chrome/browser/browser_main_gtk.h"

class BrowserMainPartsChromeos : public BrowserMainPartsGtk {
 public:
  explicit BrowserMainPartsChromeos(const MainFunctionParams& parameters)
      : BrowserMainPartsGtk(parameters) {}

 protected:
  virtual void PostMainMessageLoopStart();
  DISALLOW_COPY_AND_ASSIGN(BrowserMainPartsChromeos);
};

#endif  // CHROME_BROWSER_CHROMEOS_BROWSER_MAIN_CHROMEOS_H_
