// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {

// GeolocationDispatcherHost is an observer for Geolocation permissions-related
// messages. In this role, it's the complement of GeolocationDispatcher (owned
// by RenderFrame).
// TODO(blundell): Eliminate this class in favor of having
// Mojo handle permissions for geolocation once there is resolution on how
// that will work. crbug.com/420498
class GeolocationDispatcherHost : public WebContentsObserver {
 public:
  explicit GeolocationDispatcherHost(WebContents* web_contents);
  ~GeolocationDispatcherHost() override;

 private:
  // WebContentsObserver
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                                   const LoadCommittedDetails& details,
                                   const FrameNavigateParams& params) override;
  bool OnMessageReceived(const IPC::Message& msg,
                         RenderFrameHost* render_frame_host) override;

  // Message handlers:
  // TODO(mlamouri): |requesting_origin| should be a security origin to
  // guarantee that a proper origin is passed.
  void OnRequestPermission(RenderFrameHost* render_frame_host,
                           int bridge_id,
                           const GURL& requesting_origin,
                           bool user_gesture);

  void SendGeolocationPermissionResponse(int render_process_id,
                                         int render_frame_id,
                                         int bridge_id,
                                         bool allowed);

  // Clear pending permissions associated with a given frame and request the
  // browser to cancel the permission requests.
  void CancelPermissionRequestsForFrame(RenderFrameHost* render_frame_host);

  struct PendingPermission {
    PendingPermission(int render_frame_id,
                      int render_process_id,
                      int bridge_id,
                      const GURL& origin);
    ~PendingPermission();
    int render_frame_id;
    int render_process_id;
    int bridge_id;
    GURL origin;
  };
  std::vector<PendingPermission> pending_permissions_;

  base::WeakPtrFactory<GeolocationDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
