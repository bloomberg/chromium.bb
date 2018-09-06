// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_LAUNCHER_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_LAUNCHER_H_

#include "base/macros.h"

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class PrefService;
class GURL;

namespace browser_switcher {

// Used to launch an appropriate alternative browser based on policy/pref
// values.
//
// Delegates I/O operations to an |AlternativeBrowserDriver|.
class AlternativeBrowserLauncher : public content::WebContentsObserver {
 public:
  explicit AlternativeBrowserLauncher(PrefService* prefs);
  AlternativeBrowserLauncher(PrefService* prefs,
                             std::unique_ptr<AlternativeBrowserDriver> driver);
  ~AlternativeBrowserLauncher() override;

  // Opens |url| in an alternative browser.  Returns true on success, false on
  // error.
  bool Launch(const GURL& url) const;

 private:
  void OnAltBrowserPathChanged();
  void OnAltBrowserParametersChanged();

  PrefService* const prefs_;
  PrefChangeRegistrar change_registrar_;

  const std::unique_ptr<AlternativeBrowserDriver> driver_;

  DISALLOW_COPY_AND_ASSIGN(AlternativeBrowserLauncher);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_LAUNCHER_H_
