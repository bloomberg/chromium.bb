// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/common/content_export.h"

namespace content {
class ResourceRequestBody;

// PlzNavigate
// A UI thread object that owns a navigation request until it commits. It
// ensures the UI thread can start a navigation request in the
// ResourceDispatcherHost (that lives on the IO thread).
// TODO(clamy): Describe the interactions between the UI and IO thread during
// the navigation following its refactoring.
class NavigationRequest {
 public:
  NavigationRequest(const NavigationRequestInfo& info,
                    int64 frame_tree_node_id);

  ~NavigationRequest();

  // Called on the UI thread by the RenderFrameHostManager which owns the
  // NavigationRequest. After calling this function, |body| can no longer be
  // manipulated on the UI thread.
  void BeginNavigation(scoped_refptr<ResourceRequestBody> body);

  // Called on the UI thread by the RenderFrameHostManager which owns the
  // NavigationRequest whenever this navigation request should be canceled.
  void CancelNavigation();

  const NavigationRequestInfo& info() const { return info_; }

  int64 frame_tree_node_id() const { return frame_tree_node_id_; }

  int64 navigation_request_id() const { return navigation_request_id_; }

 private:
  const int64 navigation_request_id_;
  const NavigationRequestInfo info_;
  const int64 frame_tree_node_id_;

  DISALLOW_COPY_AND_ASSIGN(NavigationRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
