// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/browser_side_navigation_policy.h"

#include "base/command_line.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/features.h"

namespace content {

bool IsBrowserSideNavigationEnabled() {
  return true;
}

// Browser side navigation (aka PlzNavigate) is using blob URLs to deliver
// the body of the main resource to the renderer process. When enabled, the
// NavigationMojoResponse feature replaces this mechanism by a Mojo DataPipe.
// Design doc: https://goo.gl/Rrrc7n.
bool IsNavigationMojoResponseEnabled() {
  if (!IsBrowserSideNavigationEnabled())
    return false;

  return base::FeatureList::IsEnabled(features::kNavigationMojoResponse) ||
         base::FeatureList::IsEnabled(
             features::kServiceWorkerServicification) ||
         base::FeatureList::IsEnabled(features::kSignedHTTPExchange) ||
         base::FeatureList::IsEnabled(network::features::kNetworkService);
}

}  // namespace content
