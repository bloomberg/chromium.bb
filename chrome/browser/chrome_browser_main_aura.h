// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_AURA_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_AURA_H_

#include "chrome/browser/chrome_browser_main.h"

class ChromeBrowserMainPartsAura : public ChromeBrowserMainParts {
 public:
  explicit ChromeBrowserMainPartsAura(const MainFunctionParams& parameters);

  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsAura);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_AURA_H_
