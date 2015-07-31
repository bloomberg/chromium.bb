// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_H_
#define CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/child/permissions/permission_observers_registry.h"
#include "content/common/permission_service.mojom.h"
#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionClient.h"

namespace content {

class ServiceRegistry;

// The PermissionDispatcher is a layer between Blink and the Mojo
// PermissionService. It implements blink::WebPermissionClient. It is being used
// from workers and frames independently. When called outside of the main
// thread, QueryPermissionForWorker is meant to be called. It will handle the
// thread jumping.
class PermissionDispatcher : public blink::WebPermissionClient,
                             public PermissionObserversRegistry {
 public:
  // Returns whether the given WebPermissionType is observable. Some types have
  // static values that never changes.
  static bool IsObservable(blink::WebPermissionType type);

  // The caller must guarantee that |service_registry| will have a lifetime
  // larger than this instance of PermissionDispatcher.
  explicit PermissionDispatcher(ServiceRegistry* service_registry);
  virtual ~PermissionDispatcher();

  // blink::WebPermissionClient implementation.
  virtual void queryPermission(blink::WebPermissionType type,
                               const blink::WebURL& origin,
                               blink::WebPermissionCallback* callback);
  virtual void requestPermission(blink::WebPermissionType,
                                 const blink::WebURL& origin,
                                 blink::WebPermissionCallback* callback);
  virtual void revokePermission(blink::WebPermissionType,
                                const blink::WebURL& origin,
                                blink::WebPermissionCallback* callback);
  virtual void startListening(blink::WebPermissionType type,
                              const blink::WebURL& origin,
                              blink::WebPermissionObserver* observer);
  virtual void stopListening(blink::WebPermissionObserver* observer);

  // The following methods must be called by workers on the main thread.
  void QueryPermissionForWorker(blink::WebPermissionType type,
                                const std::string& origin,
                                blink::WebPermissionCallback* callback,
                                int worker_thread_id);
  void RequestPermissionForWorker(blink::WebPermissionType type,
                                  const std::string& origin,
                                  blink::WebPermissionCallback* callback,
                                  int worker_thread_id);
  void RevokePermissionForWorker(blink::WebPermissionType type,
                                 const std::string& origin,
                                 blink::WebPermissionCallback* callback,
                                 int worker_thread_id);
  void StartListeningForWorker(
      blink::WebPermissionType type,
      const std::string& origin,
      int worker_thread_id,
      const base::Callback<void(blink::WebPermissionStatus)>& callback);
  void GetNextPermissionChangeForWorker(
      blink::WebPermissionType type,
      const std::string& origin,
      blink::WebPermissionStatus status,
      int worker_thread_id,
      const base::Callback<void(blink::WebPermissionStatus)>& callback);

 private:
  // Runs the given |callback| with |status| as a parameter. It has to be run
  // on a worker thread.
  static void RunCallbackOnWorkerThread(
      blink::WebPermissionCallback* callback,
      scoped_ptr<blink::WebPermissionStatus> status);

  // Helper method that returns an initialized PermissionServicePtr.
  PermissionServicePtr& GetPermissionServicePtr();

  void QueryPermissionInternal(blink::WebPermissionType type,
                               const std::string& origin,
                               blink::WebPermissionCallback* callback,
                               int worker_thread_id);
  void RequestPermissionInternal(blink::WebPermissionType type,
                                const std::string& origin,
                                blink::WebPermissionCallback* callback,
                                int worker_thread_id);
  void RevokePermissionInternal(blink::WebPermissionType type,
                                const std::string& origin,
                                blink::WebPermissionCallback* callback,
                                int worker_thread_id);

  // This is the callback function used for query and revoke
  void OnPermissionResponse(int request_id,
                            PermissionStatus status);
  void OnPermissionChanged(blink::WebPermissionType type,
                           const std::string& origin,
                           blink::WebPermissionObserver* observer,
                           PermissionStatus status);
  void OnPermissionChangedForWorker(
      int worker_thread_id,
      const base::Callback<void(blink::WebPermissionStatus)>& callback,
      PermissionStatus status);

  void GetNextPermissionChange(blink::WebPermissionType type,
                               const std::string& origin,
                               blink::WebPermissionObserver* observer,
                               PermissionStatus current_status);

  // Saves some basic information about the callback in order to be able to run
  // it in the right thread.
  class CallbackInformation {
   public:
    CallbackInformation(blink::WebPermissionCallback* callback,
                        int worker_thread_id);
    ~CallbackInformation();

    blink::WebPermissionCallback* callback() const;
    int worker_thread_id() const;

    blink::WebPermissionCallback* ReleaseCallback();

   private:
    scoped_ptr<blink::WebPermissionCallback> callback_;
    int worker_thread_id_;

    DISALLOW_COPY_AND_ASSIGN(CallbackInformation);
  };
  using CallbackMap = IDMap<CallbackInformation, IDMapOwnPointer>;
  CallbackMap pending_callbacks_;

  ServiceRegistry* service_registry_;
  PermissionServicePtr permission_service_;

  DISALLOW_COPY_AND_ASSIGN(PermissionDispatcher);
};

}  // namespace content

#endif // CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_H_
