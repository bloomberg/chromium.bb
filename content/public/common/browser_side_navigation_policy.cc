// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/browser_side_navigation_policy.h"

#include "base/command_line.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

bool IsBrowserSideNavigationEnabled() {
  // First check if the network service is enabled or if PlzNavigate was
  // manually enabled via the --enable-browser-side-navigation flag. This takes
  // precedence over other configuration. This ensure that tests that require
  // PlzNavigate and manually set the command line flag or enable the network
  // service will still work even if the test suite is ran with
  // --disable-browser-side-navigation.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation) ||
      base::FeatureList::IsEnabled(features::kNetworkService)) {
    return true;
  }

  // Otherwise, disabling PlzNavigate through the command line flag
  // --disable-browser-side-navigation has priority over the feature being
  // enabled through the feature list. This ensures that test suites that run
  // with --disable-browser-side-navigation do run with browser-side navigation
  // disabled, even if the feature is enabled by default.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBrowserSideNavigation)) {
    return false;
  }

  // Otherwise, default to the feature list.
  return base::FeatureList::IsEnabled(features::kBrowserSideNavigation);
}

// Browser side navigation (aka PlzNavigate) is using blob URLs to deliver
// the body of the main resource to the renderer process. When enabled, the
// NavigationMojoResponse feature replaces this mechanism by a Mojo DataPipe.
// Design doc: https://goo.gl/Rrrc7n.
bool IsNavigationMojoResponseEnabled() {
  if (!IsBrowserSideNavigationEnabled())
    return false;

  return base::FeatureList::IsEnabled(features::kNavigationMojoResponse);
}

}  // namespace content
