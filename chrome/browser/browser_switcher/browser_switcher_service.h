// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

#include <memory>

class PrefService;

namespace browser_switcher {

class AlternativeBrowserLauncher;
class BrowserSwitcherSitelist;

// Manages resources that can be accessed from a |BrowserSwitcherTabHelper|.
class BrowserSwitcherService : public KeyedService {
 public:
  explicit BrowserSwitcherService(PrefService* prefs);
  ~BrowserSwitcherService() override;

  AlternativeBrowserLauncher* launcher();
  BrowserSwitcherSitelist* sitelist();

  void SetLauncherForTesting(
      std::unique_ptr<AlternativeBrowserLauncher> launcher);
  void SetSitelistForTesting(std::unique_ptr<BrowserSwitcherSitelist> sitelist);

 private:
  std::unique_ptr<AlternativeBrowserLauncher> launcher_;
  std::unique_ptr<BrowserSwitcherSitelist> sitelist_;

  PrefService* const prefs_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherService);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
