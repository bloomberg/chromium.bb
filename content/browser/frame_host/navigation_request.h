// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/frame_host/navigation_request_info.h"

namespace content {
class ResourceRequestBody;

// A UI thread object that owns a navigation request until it commits. It
// ensures the UI thread can start a navigation request in the
// ResourceDispatcherHost (that lives on the IO thread).
// TODO(clamy): Describe the interactions between the UI and IO thread during
// the navigation following its refactoring.
class NavigationRequest {
 public:
  NavigationRequest(const NavigationRequestInfo& info, int64 frame_node_id);

  ~NavigationRequest();

  const NavigationRequestInfo& info_for_testing() const { return info_; }
  int64 frame_node_id() const { return frame_node_id_; }

  // Called on the UI thread by the RenderFrameHostManager which owns the
  // NavigationRequest. After calling this function, |body| can no longer be
  // manipulated on the UI thread.
  void BeginNavigation(scoped_refptr<ResourceRequestBody> body);

 private:
  const NavigationRequestInfo info_;
  const int64 frame_node_id_;

  DISALLOW_COPY_AND_ASSIGN(NavigationRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
