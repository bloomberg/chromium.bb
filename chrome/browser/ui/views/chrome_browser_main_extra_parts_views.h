// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeBrowserMainExtraPartsViews : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsViews();

  // Overridden from ChromeBrowserMainExtraParts:
  virtual void ToolkitInitialized() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
