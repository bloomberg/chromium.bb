// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_FILTERING_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_FILTERING_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;

// NavigationThrottle responsible for filtering subframe document loads, which
// are considered subresource loads of their parent frame, hence are subject to
// subresource filtering using the parent frame's
// AsyncDocumentSubresourceFilter.
//
// The throttle should only be instantiated for navigations occuring in
// subframes owned by documents which already have filtering activated, and
// therefore an associated (Async)DocumentSubresourceFilter.
class SubframeNavigationFilteringThrottle : public content::NavigationThrottle {
 public:
  SubframeNavigationFilteringThrottle(
      content::NavigationHandle* handle,
      AsyncDocumentSubresourceFilter* parent_frame_filter);
  ~SubframeNavigationFilteringThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;

 private:
  content::NavigationThrottle::ThrottleCheckResult DeferToCalculateLoadPolicy();
  void OnCalculatedLoadPolicy(LoadPolicy policy);

  // Must outlive this class.
  AsyncDocumentSubresourceFilter* parent_frame_filter_;

  base::WeakPtrFactory<SubframeNavigationFilteringThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SubframeNavigationFilteringThrottle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_FILTERING_THROTTLE_H_
