// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/permissions/permission_dispatcher_thread_proxy.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_local.h"
#include "content/child/permissions/permission_dispatcher.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using base::LazyInstance;
using base::ThreadLocalPointer;

namespace content {

namespace {

LazyInstance<ThreadLocalPointer<PermissionDispatcherThreadProxy>>::Leaky
    g_permission_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

PermissionDispatcherThreadProxy*
PermissionDispatcherThreadProxy::GetThreadInstance(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    PermissionDispatcher* permission_dispatcher) {
  if (g_permission_dispatcher_tls.Pointer()->Get())
    return g_permission_dispatcher_tls.Pointer()->Get();

  PermissionDispatcherThreadProxy* instance =
      new PermissionDispatcherThreadProxy(main_thread_task_runner,
                                        permission_dispatcher);
  DCHECK(WorkerTaskRunner::Instance()->CurrentWorkerId());
  WorkerTaskRunner::Instance()->AddStopObserver(instance);
  return instance;
}

PermissionDispatcherThreadProxy::PermissionDispatcherThreadProxy(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    PermissionDispatcher* permission_dispatcher)
  : main_thread_task_runner_(main_thread_task_runner),
    permission_dispatcher_(permission_dispatcher) {
  g_permission_dispatcher_tls.Pointer()->Set(this);
}

PermissionDispatcherThreadProxy::~PermissionDispatcherThreadProxy() {
  g_permission_dispatcher_tls.Pointer()->Set(nullptr);
}

void PermissionDispatcherThreadProxy::queryPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionQueryCallback* callback) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PermissionDispatcher::QueryPermissionForWorker,
                 base::Unretained(permission_dispatcher_),
                 type,
                 origin.string().utf8(),
                 base::Unretained(callback),
                 WorkerTaskRunner::Instance()->CurrentWorkerId()));
}

void PermissionDispatcherThreadProxy::OnWorkerRunLoopStopped() {
  delete this;
}

}  // namespace content
