// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_permission_dispatcher_proxy.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
#include "url/gurl.h"

namespace content {

class MediaPermissionDispatcherProxy::Core {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       base::WeakPtr<MediaPermissionDispatcher> parent,
       base::WeakPtr<MediaPermissionDispatcher> media_permission_dispatcher);
  ~Core();

  void HasPermission(Type type,
                     const GURL& security_origin,
                     uint32_t request_id);

  void RequestPermission(Type type,
                         const GURL& security_origin,
                         uint32_t request_id);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() {
    return caller_task_runner_;
  }

  base::WeakPtr<MediaPermissionDispatcherProxy::Core> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Reports the result on the caller's thread.
  void ReportResult(uint32_t request_id, bool granted);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // |parent_| is the proxy which holds this Core.
  base::WeakPtr<MediaPermissionDispatcher> parent_;

  // |media_permission_| is the real implementation which calls
  // PermissionServices on the UI thread.
  base::WeakPtr<MediaPermissionDispatcher> media_permission_;

  base::WeakPtrFactory<MediaPermissionDispatcherProxy::Core> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MediaPermissionDispatcherProxy::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::WeakPtr<MediaPermissionDispatcher> parent,
    base::WeakPtr<MediaPermissionDispatcher> media_permission)
    : ui_task_runner_(ui_task_runner),
      caller_task_runner_(caller_task_runner),
      parent_(parent),
      media_permission_(media_permission),
      weak_ptr_factory_(this) {}

MediaPermissionDispatcherProxy::Core::~Core() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
}

void MediaPermissionDispatcherProxy::Core::HasPermission(
    Type type,
    const GURL& security_origin,
    uint32_t request_id) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (media_permission_.get()) {
    media_permission_->HasPermission(
        type, security_origin,
        base::Bind(&MediaPermissionDispatcherProxy::Core::ReportResult,
                   GetWeakPtr(), request_id));
  } else {
    ReportResult(request_id, false);
  }
}

void MediaPermissionDispatcherProxy::Core::RequestPermission(
    Type type,
    const GURL& security_origin,
    uint32_t request_id) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (media_permission_.get()) {
    media_permission_->RequestPermission(
        type, security_origin,
        base::Bind(&MediaPermissionDispatcherProxy::Core::ReportResult,
                   GetWeakPtr(), request_id));
  } else {
    ReportResult(request_id, false);
  }
}

void MediaPermissionDispatcherProxy::Core::ReportResult(uint32_t request_id,
                                                        bool result) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  caller_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MediaPermissionDispatcherProxy::DeliverResult,
                            parent_, request_id, result));
}

MediaPermissionDispatcherProxy::MediaPermissionDispatcherProxy(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::WeakPtr<MediaPermissionDispatcher> media_permission)
    : weak_ptr_factory_(this) {
  thread_checker().DetachFromThread();
  core_.reset(new Core(ui_task_runner, caller_task_runner,
                       weak_ptr_factory_.GetWeakPtr(), media_permission));
}

MediaPermissionDispatcherProxy::~MediaPermissionDispatcherProxy() {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());

  auto ui_task_runner = core_->ui_task_runner();
  ui_task_runner->DeleteSoon(FROM_HERE, core_.release());
}

void MediaPermissionDispatcherProxy::HasPermission(
    Type type,
    const GURL& security_origin,
    const PermissionStatusCB& permission_status_cb) {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());

  core_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Core::HasPermission, core_->GetWeakPtr(), type,
                 security_origin, RegisterCallback(permission_status_cb)));
}

void MediaPermissionDispatcherProxy::RequestPermission(
    Type type,
    const GURL& security_origin,
    const PermissionStatusCB& permission_status_cb) {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());

  core_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Core::RequestPermission, core_->GetWeakPtr(), type,
                 security_origin, RegisterCallback(permission_status_cb)));
}

}  // namespace content
