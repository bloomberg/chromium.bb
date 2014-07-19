// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

#if defined(USE_AURA)
namespace wm {
class WMState;
}
#endif

class ChromeBrowserMainExtraPartsViews : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsViews();
  virtual ~ChromeBrowserMainExtraPartsViews();

  // Overridden from ChromeBrowserMainExtraParts:
  virtual void ToolkitInitialized() OVERRIDE;

 private:
#if defined(USE_AURA)
  scoped_ptr<wm::WMState> wm_state_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
