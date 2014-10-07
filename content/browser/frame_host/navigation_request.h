// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"

namespace content {

class FrameTreeNode;
class ResourceRequestBody;
struct NavigationRequestInfo;

// PlzNavigate
// A UI thread object that owns a navigation request until it commits. It
// ensures the UI thread can start a navigation request in the
// ResourceDispatcherHost (that lives on the IO thread).
// TODO(clamy): Describe the interactions between the UI and IO thread during
// the navigation following its refactoring.
class CONTENT_EXPORT NavigationRequest {
 public:
  NavigationRequest(FrameTreeNode* frame_tree_node,
                    const CommonNavigationParams& common_params,
                    const CommitNavigationParams& commit_params);

  ~NavigationRequest();

  // Called on the UI thread by the RenderFrameHostManager which owns the
  // NavigationRequest. Takes ownership of |info|. After calling this function,
  // |body| can no longer be manipulated on the UI thread.
  void BeginNavigation(scoped_ptr<NavigationRequestInfo> info,
                       scoped_refptr<ResourceRequestBody> body);

  CommonNavigationParams& common_params() { return common_params_; }

  const CommitNavigationParams& commit_params() const { return commit_params_; }

  NavigationRequestInfo* info_for_test() const { return info_.get(); }

 private:
  FrameTreeNode* frame_tree_node_;

  // Initialized on creation of the NavigationRequest. Sent to the renderer when
  // the navigation is ready to commit.
  // Note: When the navigation is ready to commit, the url in |common_params|
  // will be set to the final navigation url, obtained after following all
  // redirects.
  CommonNavigationParams common_params_;
  const CommitNavigationParams commit_params_;

  // Initialized when beginning the navigation.
  scoped_ptr<NavigationRequestInfo> info_;

  DISALLOW_COPY_AND_ASSIGN(NavigationRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
