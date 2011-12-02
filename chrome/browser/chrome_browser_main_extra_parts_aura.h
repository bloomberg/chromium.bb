// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_AURA_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_AURA_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeBrowserMainExtraPartsAura : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsAura();

  virtual void PostBrowserProcessInit() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsAura);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_AURA_H_
