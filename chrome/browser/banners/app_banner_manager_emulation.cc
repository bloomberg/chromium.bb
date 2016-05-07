// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_emulation.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_data_fetcher_desktop.h"
#include "ui/display/screen.h"

namespace {

// We need to provide web developers with the guidance on the minimum icon
// size they should list in the manifest. Since the size depends on the
// device, we pick the baseline being a Nexus 5X-alike device.
// We make it clear in the 'add to home screen' emulation UI that the
// size does not correspond to the currently emulated device, but rather is
// a generic baseline.
const int kMinimumIconSize = 144;

}  // anonymous namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(banners::AppBannerManagerEmulation);

namespace banners {

AppBannerDataFetcher* AppBannerManagerEmulation::CreateAppBannerDataFetcher(
    base::WeakPtr<AppBannerDataFetcher::Delegate> weak_delegate,
    bool is_debug_mode) {

  // Divide by the current screen density as the data fetcher will multiply
  // it back in when trying to fetch an appropriate icon
  int size = kMinimumIconSize / display::Screen::GetScreen()->
      GetPrimaryDisplay().device_scale_factor();

  return new AppBannerDataFetcherDesktop(web_contents(), weak_delegate,
                                         size, size, is_debug_mode);
}

AppBannerManagerEmulation::AppBannerManagerEmulation(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) {
}

}  // namespace banners
