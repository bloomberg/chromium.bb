// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_THREAD_PROXY_H_
#define CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_THREAD_PROXY_H_

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionClient.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class PermissionDispatcher;

class PermissionDispatcherThreadProxy :
    public blink::WebPermissionClient,
    public WorkerTaskRunner::Observer {
 public:
  static PermissionDispatcherThreadProxy* GetThreadInstance(
      base::SingleThreadTaskRunner* main_thread_task_runner,
      PermissionDispatcher* permissions_dispatcher);

  // blink::WebPermissionClient implementation.
  virtual void queryPermission(blink::WebPermissionType type,
                               const blink::WebURL& origin,
                               blink::WebPermissionQueryCallback* callback);

  // WorkerTaskRunner::Observer implementation.
  void OnWorkerRunLoopStopped() override;

 private:
  PermissionDispatcherThreadProxy(
      base::SingleThreadTaskRunner* main_thread_task_runner,
      PermissionDispatcher* permissions_dispatcher);

  virtual ~PermissionDispatcherThreadProxy();

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  PermissionDispatcher* permission_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PermissionDispatcherThreadProxy);
};

}  // namespace content

#endif // CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_THREAD_PROXY_H_
