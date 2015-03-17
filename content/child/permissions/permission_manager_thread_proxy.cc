// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/permissions/permission_manager_thread_proxy.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_local.h"
#include "content/child/permissions/permission_manager.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using base::LazyInstance;
using base::ThreadLocalPointer;

namespace content {

namespace {

LazyInstance<ThreadLocalPointer<PermissionManagerThreadProxy>>::Leaky
    g_permission_manager_tls = LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

PermissionManagerThreadProxy*
PermissionManagerThreadProxy::GetThreadInstance(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    PermissionManager* permissions_manager) {
  if (g_permission_manager_tls.Pointer()->Get())
    return g_permission_manager_tls.Pointer()->Get();

  PermissionManagerThreadProxy* instance =
      new PermissionManagerThreadProxy(main_thread_task_runner,
                                        permissions_manager);
  DCHECK(WorkerTaskRunner::Instance()->CurrentWorkerId());
  WorkerTaskRunner::Instance()->AddStopObserver(instance);
  return instance;
}

PermissionManagerThreadProxy::PermissionManagerThreadProxy(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    PermissionManager* permissions_manager)
  : main_thread_task_runner_(main_thread_task_runner),
    permissions_manager_(permissions_manager) {
  g_permission_manager_tls.Pointer()->Set(this);
}

PermissionManagerThreadProxy::~PermissionManagerThreadProxy() {
  g_permission_manager_tls.Pointer()->Set(nullptr);
}

void PermissionManagerThreadProxy::queryPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionQueryCallback* callback) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PermissionManager::QueryPermissionForWorker,
                 base::Unretained(permissions_manager_),
                 type,
                 origin.string().utf8(),
                 base::Unretained(callback),
                 WorkerTaskRunner::Instance()->CurrentWorkerId()));
}

void PermissionManagerThreadProxy::OnWorkerRunLoopStopped() {
  delete this;
}

}  // namespace content
