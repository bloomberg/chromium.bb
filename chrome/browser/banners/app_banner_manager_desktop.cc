// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_data_fetcher_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/constants.h"

namespace {
// TODO(dominickn) Identify the best minimum icon size to guarantee the best
// user experience.
int kMinimumIconSize = extension_misc::EXTENSION_ICON_LARGE;
}  // anonymous namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(banners::AppBannerManagerDesktop);

namespace banners {

bool AppBannerManagerDesktop::IsEnabled() {
#if defined(OS_CHROMEOS)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableAddToShelf);
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAddToShelf);
#endif
}

AppBannerDataFetcher* AppBannerManagerDesktop::CreateAppBannerDataFetcher(
    base::WeakPtr<AppBannerDataFetcher::Delegate> weak_delegate,
    bool is_debug_mode) {
  return new AppBannerDataFetcherDesktop(web_contents(), weak_delegate,
                                         kMinimumIconSize, kMinimumIconSize,
                                         is_debug_mode);
}

AppBannerManagerDesktop::AppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) {
}

}  // namespace banners
