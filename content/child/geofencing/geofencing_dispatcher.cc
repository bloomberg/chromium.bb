// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/geofencing/geofencing_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/geofencing_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"
#include "third_party/WebKit/public/platform/WebGeofencingError.h"
#include "third_party/WebKit/public/platform/WebGeofencingRegistration.h"

using blink::WebGeofencingError;

namespace content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<void>>::Leaky g_dispatcher_tls =
    LAZY_INSTANCE_INITIALIZER;

void* const kHasBeenDeleted = reinterpret_cast<void*>(0x1);

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
}

}  // namespace

GeofencingDispatcher::GeofencingDispatcher(ThreadSafeSender* sender)
    : thread_safe_sender_(sender) {
  g_dispatcher_tls.Pointer()->Set(static_cast<void*>(this));
}

GeofencingDispatcher::~GeofencingDispatcher() {
  g_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

bool GeofencingDispatcher::Send(IPC::Message* msg) {
  return thread_safe_sender_->Send(msg);
}

void GeofencingDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GeofencingDispatcher, msg)
    IPC_MESSAGE_HANDLER(GeofencingMsg_RegisterRegionComplete,
                        OnRegisterRegionComplete)
    IPC_MESSAGE_HANDLER(GeofencingMsg_UnregisterRegionComplete,
                        OnUnregisterRegionComplete)
    IPC_MESSAGE_HANDLER(GeofencingMsg_GetRegisteredRegionsComplete,
                        OnGetRegisteredRegionsComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

void GeofencingDispatcher::RegisterRegion(
    const blink::WebString& region_id,
    const blink::WebCircularGeofencingRegion& region,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebGeofencingCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = region_registration_requests_.Add(callbacks);
  // TODO(mek): Immediately reject requests lacking a service worker
  // registration, without bouncing through browser process.
  int64_t serviceworker_registration_id = kInvalidServiceWorkerRegistrationId;
  if (service_worker_registration) {
    serviceworker_registration_id =
        static_cast<WebServiceWorkerRegistrationImpl*>(
            service_worker_registration)->registration_id();
  }
  Send(new GeofencingHostMsg_RegisterRegion(CurrentWorkerId(),
                                            request_id,
                                            region_id.utf8(),
                                            region,
                                            serviceworker_registration_id));
}

void GeofencingDispatcher::UnregisterRegion(
    const blink::WebString& region_id,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebGeofencingCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = region_unregistration_requests_.Add(callbacks);
  // TODO(mek): Immediately reject requests lacking a service worker
  // registration, without bouncing through browser process.
  int64_t serviceworker_registration_id = kInvalidServiceWorkerRegistrationId;
  if (service_worker_registration) {
    serviceworker_registration_id =
        static_cast<WebServiceWorkerRegistrationImpl*>(
            service_worker_registration)->registration_id();
  }
  Send(new GeofencingHostMsg_UnregisterRegion(CurrentWorkerId(),
                                              request_id,
                                              region_id.utf8(),
                                              serviceworker_registration_id));
}

void GeofencingDispatcher::GetRegisteredRegions(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebGeofencingRegionsCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = get_registered_regions_requests_.Add(callbacks);
  // TODO(mek): Immediately reject requests lacking a service worker
  // registration, without bouncing through browser process.
  int64_t serviceworker_registration_id = kInvalidServiceWorkerRegistrationId;
  if (service_worker_registration) {
    serviceworker_registration_id =
        static_cast<WebServiceWorkerRegistrationImpl*>(
            service_worker_registration)->registration_id();
  }
  Send(new GeofencingHostMsg_GetRegisteredRegions(
      CurrentWorkerId(), request_id, serviceworker_registration_id));
}

void GeofencingDispatcher::SetMockProvider(bool service_available) {
  Send(new GeofencingHostMsg_SetMockProvider(
      service_available ? GeofencingMockState::SERVICE_AVAILABLE
                        : GeofencingMockState::SERVICE_UNAVAILABLE));
}

void GeofencingDispatcher::ClearMockProvider() {
  Send(new GeofencingHostMsg_SetMockProvider(GeofencingMockState::NONE));
}

void GeofencingDispatcher::SetMockPosition(double latitude, double longitude) {
  Send(new GeofencingHostMsg_SetMockPosition(latitude, longitude));
}

GeofencingDispatcher* GeofencingDispatcher::GetOrCreateThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender) {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS GeofencingDispatcher.";
    g_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_dispatcher_tls.Pointer()->Get())
    return static_cast<GeofencingDispatcher*>(
        g_dispatcher_tls.Pointer()->Get());

  GeofencingDispatcher* dispatcher =
      new GeofencingDispatcher(thread_safe_sender);
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

GeofencingDispatcher* GeofencingDispatcher::GetThreadSpecificInstance() {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted)
    return NULL;
  return static_cast<GeofencingDispatcher*>(g_dispatcher_tls.Pointer()->Get());
}

void GeofencingDispatcher::OnRegisterRegionComplete(int thread_id,
                                                    int request_id,
                                                    GeofencingStatus status) {
  blink::WebGeofencingCallbacks* callbacks =
      region_registration_requests_.Lookup(request_id);
  DCHECK(callbacks);

  if (status == GEOFENCING_STATUS_OK) {
    callbacks->onSuccess();
  } else {
    callbacks->onError(WebGeofencingError(
        WebGeofencingError::ErrorTypeAbort,
        blink::WebString::fromUTF8(GeofencingStatusToString(status))));
  }
  region_registration_requests_.Remove(request_id);
}

void GeofencingDispatcher::OnUnregisterRegionComplete(int thread_id,
                                                      int request_id,
                                                      GeofencingStatus status) {
  blink::WebGeofencingCallbacks* callbacks =
      region_unregistration_requests_.Lookup(request_id);
  DCHECK(callbacks);

  if (status == GEOFENCING_STATUS_OK) {
    callbacks->onSuccess();
  } else {
    callbacks->onError(WebGeofencingError(
        WebGeofencingError::ErrorTypeAbort,
        blink::WebString::fromUTF8(GeofencingStatusToString(status))));
  }
  region_unregistration_requests_.Remove(request_id);
}

void GeofencingDispatcher::OnGetRegisteredRegionsComplete(
    int thread_id,
    int request_id,
    GeofencingStatus status,
    const GeofencingRegistrations& regions) {
  blink::WebGeofencingRegionsCallbacks* callbacks =
      get_registered_regions_requests_.Lookup(request_id);
  DCHECK(callbacks);

  if (status == GEOFENCING_STATUS_OK) {
    blink::WebVector<blink::WebGeofencingRegistration> result(regions.size());
    size_t index = 0;
    for (GeofencingRegistrations::const_iterator it = regions.begin();
         it != regions.end();
         ++it, ++index) {
      result[index].id = blink::WebString::fromUTF8(it->first);
      result[index].region = it->second;
    }
    callbacks->onSuccess(result);
  } else {
    callbacks->onError(WebGeofencingError(
        WebGeofencingError::ErrorTypeAbort,
        blink::WebString::fromUTF8(GeofencingStatusToString(status))));
  }
  get_registered_regions_requests_.Remove(request_id);
}

void GeofencingDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

}  // namespace content
