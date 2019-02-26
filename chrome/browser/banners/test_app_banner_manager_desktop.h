// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include "base/macros.h"
#include "base/optional.h"

namespace content {
class WebContents;
}

namespace banners {

// Provides the ability to await the results of the installability check that
// happens for every page load.
class TestAppBannerManagerDesktop : public AppBannerManagerDesktop {
 public:
  explicit TestAppBannerManagerDesktop(content::WebContents* web_contents);
  ~TestAppBannerManagerDesktop() override;

  static TestAppBannerManagerDesktop* CreateForWebContents(
      content::WebContents* web_contents);

  // Returns whether the installable check passed.
  bool WaitForInstallableCheck();

  // AppBannerManager:
  void OnDidGetManifest(const InstallableData& result) override;
  void OnDidPerformInstallableCheck(const InstallableData& result) override;

 private:
  void SetInstallable(bool installable);

  base::Optional<bool> installable_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestAppBannerManagerDesktop);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_TEST_APP_BANNER_MANAGER_DESKTOP_H_
