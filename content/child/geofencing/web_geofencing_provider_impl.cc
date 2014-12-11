// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/geofencing/web_geofencing_provider_impl.h"

#include "content/child/geofencing/geofencing_dispatcher.h"
#include "content/child/thread_safe_sender.h"

namespace content {

WebGeofencingProviderImpl::WebGeofencingProviderImpl(
    ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {
}

WebGeofencingProviderImpl::~WebGeofencingProviderImpl() {
}

void WebGeofencingProviderImpl::SetMockProvider(bool service_available) {
  GetDispatcher()->SetMockProvider(service_available);
}

void WebGeofencingProviderImpl::ClearMockProvider() {
  GetDispatcher()->ClearMockProvider();
}

void WebGeofencingProviderImpl::SetMockPosition(double latitude,
                                                double longitude) {
  GetDispatcher()->SetMockPosition(latitude, longitude);
}

void WebGeofencingProviderImpl::registerRegion(
    const blink::WebString& regionId,
    const blink::WebCircularGeofencingRegion& region,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebGeofencingCallbacks* callbacks) {
  GetDispatcher()->RegisterRegion(
      regionId, region, service_worker_registration, callbacks);
}

void WebGeofencingProviderImpl::unregisterRegion(
    const blink::WebString& regionId,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebGeofencingCallbacks* callbacks) {
  GetDispatcher()->UnregisterRegion(
      regionId, service_worker_registration, callbacks);
}

void WebGeofencingProviderImpl::getRegisteredRegions(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebGeofencingRegionsCallbacks* callbacks) {
  GetDispatcher()->GetRegisteredRegions(service_worker_registration, callbacks);
}

GeofencingDispatcher* WebGeofencingProviderImpl::GetDispatcher() {
  return GeofencingDispatcher::GetOrCreateThreadSpecificInstance(
      thread_safe_sender_.get());
}

}  // namespace content
