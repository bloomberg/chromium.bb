// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_

#include "base/macros.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "components/prefs/pref_change_registrar.h"

class GURL;

namespace browser_switcher {

class BrowserSwitcherPrefs;
class ParsedXml;

// Interface that decides whether a navigation should trigger a browser
// switch.
class BrowserSwitcherSitelist {
 public:
  virtual ~BrowserSwitcherSitelist();

  // Returns true if the given URL should be open in an alternative browser.
  virtual bool ShouldSwitch(const GURL& url) const = 0;

  // Set the Internet Explorer Enterprise Mode sitelist to use, in addition to
  // Chrome's sitelist/greylist policies. Consumes the object, and performs no
  // copy.
  virtual void SetIeemSitelist(ParsedXml&& sitelist) = 0;

  // Set the XML sitelist file to use, in addition to Chrome's sitelist/greylist
  // policies. This XML file is in the same format as an IEEM sitelist.
  // Consumes the object, and performs no copy.
  virtual void SetExternalSitelist(ParsedXml&& sitelist) = 0;
};

// Manages the sitelist configured by the administrator for
// BrowserSwitcher. Decides whether a navigation should trigger a browser
// switch.
class BrowserSwitcherSitelistImpl : public BrowserSwitcherSitelist {
 public:
  explicit BrowserSwitcherSitelistImpl(const BrowserSwitcherPrefs* prefs);
  ~BrowserSwitcherSitelistImpl() override;

  // BrowserSwitcherSitelist
  bool ShouldSwitch(const GURL& url) const override;
  void SetIeemSitelist(ParsedXml&& sitelist) override;
  void SetExternalSitelist(ParsedXml&& sitelist) override;

 private:
  // Returns true if there are any rules configured.
  bool IsActive() const;

  bool ShouldSwitchImpl(const GURL& url) const;

  RuleSet ieem_sitelist_;
  RuleSet external_sitelist_;

  const BrowserSwitcherPrefs* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSwitcherSitelistImpl);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SITELIST_H_
