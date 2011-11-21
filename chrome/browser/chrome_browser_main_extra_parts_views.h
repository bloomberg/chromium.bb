// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are views-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeBrowserMainExtraPartsViews : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsViews();

  virtual void ToolkitInitialized() OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
