// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_NAVIGATION_THROTTLE_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_NAVIGATION_THROTTLE_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // content

namespace subresource_filter {

// This class has two resposibilities:
// * it tracks the redirects that happen after Safe Browsing detects that the
//   page being loaded contains deceptive embedded content. If such a redirect
//   happens and leads to new domains, these are also put on the activation list
//   of the tab.
// * it creates a ContentSubresourceFilterDriver for the tab when the final site
//   of a (potentially empty) redirect chain is reached and any URL of the
//   redirect chain was on the activation list.
// This throttle is active only for main frame http or https navigations and
// doesn't act on subframe navigations or subresource loads.
class SubresourceFilterNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> Create(
      content::NavigationHandle* handle);
  ~SubresourceFilterNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillProcessResponse() override;

 private:
  explicit SubresourceFilterNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterNavigationThrottle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_NAVIGATION_THROTTLE_
