// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_GEOFENCING_GEOFENCING_DISPATCHER_H_
#define CONTENT_CHILD_GEOFENCING_GEOFENCING_DISPATCHER_H_

#include <map>
#include <string>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/geofencing_types.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/WebGeofencingProvider.h"

namespace base {
class MessageLoop;
class TaskRunner;
}

namespace IPC {
class Message;
}

namespace content {
class ThreadSafeSender;

class GeofencingDispatcher : public WorkerThread::Observer {
 public:
  explicit GeofencingDispatcher(ThreadSafeSender* sender);
  ~GeofencingDispatcher() override;

  bool Send(IPC::Message* msg);
  void OnMessageReceived(const IPC::Message& msg);

  // Corresponding to WebGeofencingProvider methods.
  void RegisterRegion(
      const blink::WebString& region_id,
      const blink::WebCircularGeofencingRegion& region,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebGeofencingCallbacks* callbacks);
  void UnregisterRegion(
      const blink::WebString& region_id,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebGeofencingCallbacks* callbacks);
  void GetRegisteredRegions(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebGeofencingRegionsCallbacks* callbacks);

  // Enables mock geofencing service. |service_available| indicates if the
  // mock service should mock geofencing being available or not.
  void SetMockProvider(bool service_available);
  // Disables mock geofencing service.
  void ClearMockProvider();
  // Set the mock geofencing position.
  void SetMockPosition(double latitude, double longitude);

  // |thread_safe_sender| needs to be passed in because if the call leads to
  // construction it will be needed.
  static GeofencingDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender);

  // Unlike GetOrCreateThreadSpecificInstance() this doesn't create a new
  // instance if thread-local instance doesn't exist.
  static GeofencingDispatcher* GetThreadSpecificInstance();

 private:
  void OnRegisterRegionComplete(int thread_id,
                                int request_id,
                                GeofencingStatus status);
  void OnUnregisterRegionComplete(int thread_id,
                                  int request_id,
                                  GeofencingStatus status);
  void OnGetRegisteredRegionsComplete(
      int thread_id,
      int request_id,
      GeofencingStatus status,
      const std::map<std::string, blink::WebCircularGeofencingRegion>& regions);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  IDMap<blink::WebGeofencingCallbacks, IDMapOwnPointer>
      region_registration_requests_;
  IDMap<blink::WebGeofencingCallbacks, IDMapOwnPointer>
      region_unregistration_requests_;
  IDMap<blink::WebGeofencingRegionsCallbacks, IDMapOwnPointer>
      get_registered_regions_requests_;

  DISALLOW_COPY_AND_ASSIGN(GeofencingDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_GEOFENCING_GEOFENCING_DISPATCHER_H_
