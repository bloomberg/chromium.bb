// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_DELEGATION_TRACKER_H_
#define CHROME_BROWSER_PERMISSIONS_DELEGATION_TRACKER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"

namespace content {

class RenderFrameHost;
enum class PermissionType;

}  // namespace content

// Keeps track of which permissions are delegated to frames. There are two
// operations possible:
// 1) Setting the permissions which are delegated to a frame by its parent, and
// 2) Querying whether a particular permission is granted to a frame.
//
// If a frame is destroyed or navigated, permissions will no longer be delegated
// to it.
class DelegationTracker {
 public:
  DelegationTracker();
  ~DelegationTracker();

  // Set the |permissions| which are delegated to |child_rfh| by its parent.
  void SetDelegatedPermissions(
      content::RenderFrameHost* child_rfh,
      const std::vector<content::PermissionType>& permissions);

  // Query whether |permission| is granted to |requesting_rfh|. This will return
  // true if |requesting_rfh| is a top-level frame or if it has been delegated
  // |permission| through its ancestor frames. Specifically, each frame on the
  // path between the main frame and |requesting_rfh| must either delegate
  // |permission| to it's child OR have the same origin as it's child on that
  // path in order for this to return true.
  bool IsGranted(content::RenderFrameHost* requesting_rfh,
                 const content::PermissionType& permission);

 private:
  class DelegatedForChild;

  void RenderFrameHostChanged(content::RenderFrameHost* rfh);

  std::unordered_map<content::RenderFrameHost*,
                     std::unique_ptr<DelegatedForChild>>
      delegated_permissions_;

  DISALLOW_COPY_AND_ASSIGN(DelegationTracker);
};

#endif  // CHROME_BROWSER_PERMISSIONS_DELEGATION_TRACKER_H_
