// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_

#include "base/macros.h"

class PrefService;
class GURL;

namespace browser_switcher {

// Interface that decides whether a navigation should trigger a browser
// switch.
class BrowserSwitcherSitelist {
 public:
  virtual ~BrowserSwitcherSitelist();

  // Returns true if the given URL should be open in an alternative browser.
  virtual bool ShouldSwitch(const GURL& url) const = 0;
};

// Manages the sitelist configured by the administrator for
// BrowserSwitcher. Decides whether a navigation should trigger a browser
// switch.
class BrowserSwitcherSitelistImpl : public BrowserSwitcherSitelist {
 public:
  explicit BrowserSwitcherSitelistImpl(PrefService* prefs);
  ~BrowserSwitcherSitelistImpl() override;

  // BrowserSwitcherSitelist
  bool ShouldSwitch(const GURL& url) const override;

 private:
  PrefService* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSwitcherSitelistImpl);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_
