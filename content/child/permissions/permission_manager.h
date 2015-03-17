// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PERMISSIONS_PERMISSION_MANAGER_H_
#define CONTENT_CHILD_PERMISSIONS_PERMISSION_MANAGER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/permission_service.mojom.h"
#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionClient.h"

namespace content {

class ServiceRegistry;

// The PermissionManager is a layer between Blink and the Mojo PermissionService
// It implements blink::WebPermissionClient. It is being used from workers and
// frames independently. When called outside of the main thread,
// QueryPermissionForWorker is meant to be called. It will handle the thread
// jumping.
class PermissionManager : public blink::WebPermissionClient {
 public:
  // The caller must guarantee that |service_registry| will have a lifetime
  // larger than this instance of PermissionManager.
  explicit PermissionManager(ServiceRegistry* service_registry);
  virtual ~PermissionManager();

  // blink::WebPermissionClient implementation.
  virtual void queryPermission(blink::WebPermissionType type,
                               const blink::WebURL& origin,
                               blink::WebPermissionQueryCallback* callback);

  void QueryPermissionForWorker(blink::WebPermissionType type,
                                const std::string& origin,
                                blink::WebPermissionQueryCallback* callback,
                                int worker_thread_id);

 protected:
  void QueryPermissionInternal(blink::WebPermissionType type,
                               const std::string& origin,
                               blink::WebPermissionQueryCallback* callback,
                               int worker_thread_id);

  void OnQueryPermission(int request_id, PermissionStatus status);

  // Called from the main thread in order to run the callback in the thread it
  // was created on.
  static void RunCallbackOnWorkerThread(
      blink::WebPermissionQueryCallback* callback,
      scoped_ptr<blink::WebPermissionStatus> status);

  // Saves some basic information about the callback in order to be able to run
  // it in the right thread.
  class CallbackInformation {
   public:
    CallbackInformation(blink::WebPermissionQueryCallback* callback,
                        int worker_thread_id);
    ~CallbackInformation();

    blink::WebPermissionQueryCallback* callback() const;
    int worker_thread_id() const;

    blink::WebPermissionQueryCallback* ReleaseCallback();

   private:
    scoped_ptr<blink::WebPermissionQueryCallback> callback_;
    int worker_thread_id_;

    DISALLOW_COPY_AND_ASSIGN(CallbackInformation);
  };
  using CallbackMap = IDMap<CallbackInformation, IDMapOwnPointer>;
  CallbackMap pending_callbacks_;

  ServiceRegistry* service_registry_;
  PermissionServicePtr permission_service_;

  DISALLOW_COPY_AND_ASSIGN(PermissionManager);
};

}  // namespace content

#endif // CONTENT_CHILD_PERMISSIONS_PERMISSION_MANAGER_H_
