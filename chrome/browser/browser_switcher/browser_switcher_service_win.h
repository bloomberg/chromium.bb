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

  void OnBrowserSwitcherPrefsChanged(BrowserSwitcherPrefs* prefs);

  static void SetIeemSitelistUrlForTesting(const std::string& url);

 private:
  // Returns the URL to fetch to get Internet Explorer's Enterprise Mode
  // sitelist, based on policy. Returns an empty (invalid) URL if IE's SiteList
  // policy is unset.
  static GURL GetIeemSitelistUrl();

  void OnIeemSitelistParsed(ParsedXml xml);

  // Save the current prefs' state to the "cache.dat" file, to be read & used by
  // the Internet Explorer BHO. This call does not block, it only posts a task
  // to a worker thread.
  void SavePrefsToFile() const;
  // Delete the "cache.dat" file created by |SavePrefsToFile()|. This call does
  // not block, it only posts a task to a worker thread.
  void DeletePrefsFile() const;

  std::unique_ptr<XmlDownloader> ieem_downloader_;

  std::unique_ptr<BrowserSwitcherPrefs::CallbackSubscription>
      prefs_subscription_;

  base::WeakPtrFactory<BrowserSwitcherServiceWin> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherServiceWin);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_WIN_H_
