// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_

#include <map>

#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/permission_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/base/media_permission.h"

namespace content {

// MediaPermission implementation in content. This class is not thread safe
// and should only be used on one thread.
class CONTENT_EXPORT MediaPermissionDispatcher : public media::MediaPermission,
                                                 public RenderFrameObserver {
 public:
  explicit MediaPermissionDispatcher(RenderFrame* render_frame);
  ~MediaPermissionDispatcher() override;

  // media::MediaPermission implementation.
  void HasPermission(Type type,
                     const GURL& security_origin,
                     const PermissionStatusCB& permission_status_cb) override;
  void RequestPermission(
      Type type,
      const GURL& security_origin,
      const PermissionStatusCB& permission_status_cb) override;

 private:
  // Map of request IDs and pending PermissionStatusCBs.
  typedef std::map<uint32_t, PermissionStatusCB> RequestMap;

  // Callback for |permission_service_| calls.
  void OnPermissionStatus(uint32_t request_id, PermissionStatus status);

  uint32_t next_request_id_;
  RequestMap requests_;
  PermissionServicePtr permission_service_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaPermissionDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_
