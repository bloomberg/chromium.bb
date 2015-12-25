// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaPermission implementation which dispatches queries/requests to Mojo
// PermissionService. It relies on its base class MediaPermissionDispatcher to
// manage pending callbacks. Non-UI threads could create
// MediaPermissionDispatcherProxy by calling CreateProxy such that calls will be
// executed on the UI thread which is where this object lives.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_IMPL_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/permission_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/media/media_permission_dispatcher.h"
#include "media/base/media_permission.h"

namespace content {

class CONTENT_EXPORT MediaPermissionDispatcherImpl
    : public MediaPermissionDispatcher,
      public RenderFrameObserver {
 public:
  explicit MediaPermissionDispatcherImpl(RenderFrame* render_frame);
  ~MediaPermissionDispatcherImpl() override;

  // media::MediaPermission implementation.
  void HasPermission(Type type,
                     const GURL& security_origin,
                     const PermissionStatusCB& permission_status_cb) override;

  // MediaStreamDevicePermissionContext doesn't support RequestPermission yet
  // and will always return CONTENT_SETTING_BLOCK.
  void RequestPermission(
      Type type,
      const GURL& security_origin,
      const PermissionStatusCB& permission_status_cb) override;

  // Create MediaPermissionDispatcherProxy object which will dispatch the
  // callback between the |caller_task_runner| and |task_runner_| in a thread
  // safe way. This can be called from non-UI thread.
  scoped_ptr<media::MediaPermission> CreateProxy(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner);

 private:
  // Callback for |permission_service_| calls.
  void OnPermissionStatus(uint32_t request_id, PermissionStatus status);

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  PermissionServicePtr permission_service_;

  base::WeakPtrFactory<MediaPermissionDispatcherImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPermissionDispatcherImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_IMPL_H_
