// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

#include <memory>

class PrefService;
class Profile;

namespace browser_switcher {

class AlternativeBrowserLauncher;
class BrowserSwitcherSitelist;
class ParsedXml;
class XmlDownloader;

// Manages per-profile resources for BrowserSwitcher.
class BrowserSwitcherService : public KeyedService {
 public:
  explicit BrowserSwitcherService(Profile* profile);
  ~BrowserSwitcherService() override;

  AlternativeBrowserLauncher* launcher();
  BrowserSwitcherSitelist* sitelist();

  void SetLauncherForTesting(
      std::unique_ptr<AlternativeBrowserLauncher> launcher);
  void SetSitelistForTesting(std::unique_ptr<BrowserSwitcherSitelist> sitelist);

  static void SetFetchDelayForTesting(base::TimeDelta delay);

#if defined(OS_WIN)
  static void SetIeemSitelistUrlForTesting(const std::string& url);
#endif

 private:
#if defined(OS_WIN)
  // Returns the URL to fetch to get Internet Explorer's Enterprise Mode
  // sitelist, based on policy. Returns an empty (invalid) URL if IE's SiteList
  // policy is unset.
  static GURL GetIeemSitelistUrl();

  void OnIeemSitelistParsed(ParsedXml xml);

  std::unique_ptr<XmlDownloader> ieem_downloader_;

  // URL to fetch the IEEM sitelist from. Only used for testing.
  static base::Optional<std::string> ieem_sitelist_url_for_testing_;
#endif

  void OnExternalSitelistParsed(ParsedXml xml);

  // Delay for the IEEM/external XML fetch tasks, launched from the constructor.
  static base::TimeDelta fetch_delay_;

  std::unique_ptr<XmlDownloader> external_sitelist_downloader_;

  // Per-profile helpers.
  std::unique_ptr<AlternativeBrowserLauncher> launcher_;
  std::unique_ptr<BrowserSwitcherSitelist> sitelist_;

  PrefService* const prefs_;

  base::WeakPtrFactory<BrowserSwitcherService> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherService);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
