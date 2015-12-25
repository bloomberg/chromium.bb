// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper class to forward the calls to MediaPermissionDispatcherImpl on UI
// thread and have the permission status callback on the caller's original
// non-UI thread.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_PROXY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_PROXY_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_permission_dispatcher.h"
#include "media/base/media_permission.h"

namespace content {

class CONTENT_EXPORT MediaPermissionDispatcherProxy
    : public MediaPermissionDispatcher {
 public:
  // |ui_task_runner| must be the UI thread. It will be used to call into the
  // |media_permission| which is the MediaPermissionDispatcherImpl.
  MediaPermissionDispatcherProxy(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      base::WeakPtr<MediaPermissionDispatcher> media_permission);

  ~MediaPermissionDispatcherProxy() override;

  // MediaPermission implementation.
  void HasPermission(Type type,
                     const GURL& security_origin,
                     const PermissionStatusCB& permission_status_cb) override;

  void RequestPermission(
      Type type,
      const GURL& security_origin,
      const PermissionStatusCB& permission_status_cb) override;

 private:
  class Core;

  void ReportResult(bool result);

  scoped_ptr<Core> core_;

  base::WeakPtrFactory<MediaPermissionDispatcherProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPermissionDispatcherProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_PROXY_H_
