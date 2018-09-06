// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_

#include "base/macros.h"

class PrefService;
class GURL;

namespace browser_switcher {

// Manages the sitelist configured by the administrator for
// BrowserSwitcher. Decides whether a navigation should trigger a browser
// switch.
class BrowserSwitcherSitelist {
 public:
  explicit BrowserSwitcherSitelist(PrefService* prefs);
  ~BrowserSwitcherSitelist();

  // Returns true if the given URL should be open in an alternative browser.
  bool ShouldSwitch(const GURL& url) const;

 private:
  PrefService* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSwitcherSitelist);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_
