// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_WIN_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_WIN_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"

namespace browser_switcher {

// Windows-specific extension of BrowserSwitcherService.
class BrowserSwitcherServiceWin : public BrowserSwitcherService {
 public:
  explicit BrowserSwitcherServiceWin(Profile* profile);
  ~BrowserSwitcherServiceWin() override;

  static void SetIeemSitelistUrlForTesting(const std::string& url);

 private:
  // Returns the URL to fetch to get Internet Explorer's Enterprise Mode
  // sitelist, based on policy. Returns an empty (invalid) URL if IE's SiteList
  // policy is unset.
  static GURL GetIeemSitelistUrl();

  void OnIeemSitelistParsed(ParsedXml xml);

  std::unique_ptr<XmlDownloader> ieem_downloader_;

  // URL to fetch the IEEM sitelist from. Only used for testing.
  static base::Optional<std::string> ieem_sitelist_url_for_testing_;

  base::WeakPtrFactory<BrowserSwitcherServiceWin> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherServiceWin);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_WIN_H_
