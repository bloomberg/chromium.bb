// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
#define CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_

// A collection of functions designed for use with content_shell based browser
// tests internal to the content/ module.
// Note: If a function here also works with browser_tests, it should be in
// the content public API.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class Shell;
class SiteInstance;
class ToRenderFrameHost;

// Navigates the frame represented by |node| to |url|, blocking until the
// navigation finishes.
void NavigateFrameToURL(FrameTreeNode* node, const GURL& url);

// Creates compact textual representations of the state of the frame tree that
// is appropriate for use in assertions.
//
// The diagrams show frame tree structure, the SiteInstance of current frames,
// presence of pending frames, and the SiteInstances of any and all proxies.
// They look like this:
//
//        Site A (D pending) -- proxies for B C
//          |--Site B --------- proxies for A C
//          +--Site C --------- proxies for B A
//               |--Site A ---- proxies for B
//               +--Site A ---- proxies for B
//                    +--Site A -- proxies for B
//       Where A = http://127.0.0.1/
//             B = http://foo.com/ (no process)
//             C = http://bar.com/
//             D = http://next.com/
//
// SiteInstances are assigned single-letter names (A, B, C) which are remembered
// across invocations of the pretty-printer.
class FrameTreeVisualizer {
 public:
  FrameTreeVisualizer();
  ~FrameTreeVisualizer();

  // Formats and returns a diagram for the provided FrameTreeNode.
  std::string DepictFrameTree(FrameTreeNode* root);

 private:
  // Assign or retrive the abbreviated short name (A, B, C) for a site instance.
  std::string GetName(SiteInstance* site_instance);

  // Elements are site instance ids. The index of the SiteInstance in the vector
  // determines the abbreviated name (0->A, 1->B) for that SiteInstance.
  std::vector<int> seen_site_instance_ids_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeVisualizer);
};

// Uses window.open to open a popup from the frame |opener| with the specified
// |url| and |name|.   Waits for the navigation to |url| to finish and then
// returns the new popup's Shell.  Note that since this navigation to |url| is
// renderer-initiated, it won't cause a process swap unless used in
// --site-per-process mode.
Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name);

// This class can be used to stall any resource request, based on an URL match.
// There is no explicit way to resume the request; it should be used carefully.
// Note: This class likely doesn't work with PlzNavigate.
// TODO(nasko): Reimplement this class using NavigationThrottle, once it has
// the ability to defer navigation requests.
class NavigationStallDelegate : public ResourceDispatcherHostDelegate {
 public:
  explicit NavigationStallDelegate(const GURL& url);

 private:
  // ResourceDispatcherHostDelegate
  void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::AppCacheService* appcache_service,
      ResourceType resource_type,
      ScopedVector<content::ResourceThrottle>* throttles) override;

  GURL url_;
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
