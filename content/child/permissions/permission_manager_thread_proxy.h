// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PERMISSIONS_PERMISSION_MANAGER_THREAD_PROXY_H_
#define CONTENT_CHILD_PERMISSIONS_PERMISSION_MANAGER_THREAD_PROXY_H_

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

class PermissionManager;

class PermissionManagerThreadProxy :
    public blink::WebPermissionClient,
    public WorkerTaskRunner::Observer {
 public:
  static PermissionManagerThreadProxy* GetThreadInstance(
      base::SingleThreadTaskRunner* main_thread_task_runner,
      PermissionManager* permissions_manager);

  // blink::WebPermissionClient implementation.
  virtual void queryPermission(blink::WebPermissionType type,
                               const blink::WebURL& origin,
                               blink::WebPermissionQueryCallback* callback);

  // WorkerTaskRunner::Observer implementation.
  void OnWorkerRunLoopStopped() override;

 private:
  PermissionManagerThreadProxy(
      base::SingleThreadTaskRunner* main_thread_task_runner,
      PermissionManager* permissions_manager);

  virtual ~PermissionManagerThreadProxy();

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  PermissionManager* permissions_manager_;

  DISALLOW_COPY_AND_ASSIGN(PermissionManagerThreadProxy);
};

}  // namespace content

#endif // CONTENT_CHILD_PERMISSIONS_PERMISSIONS_MANAGER_THREAD_PROXY_H_
