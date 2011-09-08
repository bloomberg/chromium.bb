// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_

#include "chrome/browser/chrome_browser_main_gtk.h"

class ChromeBrowserMainPartsChromeos : public ChromeBrowserMainPartsGtk {
 public:
  explicit ChromeBrowserMainPartsChromeos(const MainFunctionParams& parameters);

  virtual ~ChromeBrowserMainPartsChromeos();

  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
