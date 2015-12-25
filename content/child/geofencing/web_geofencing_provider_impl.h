// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_GEOFENCING_WEB_GEOFENCING_PROVIDER_IMPL_H_
#define CONTENT_CHILD_GEOFENCING_WEB_GEOFENCING_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGeofencingProvider.h"

namespace content {
class GeofencingDispatcher;
class ThreadSafeSender;

class CONTENT_EXPORT WebGeofencingProviderImpl
    : NON_EXPORTED_BASE(public blink::WebGeofencingProvider) {
 public:
  explicit WebGeofencingProviderImpl(ThreadSafeSender* thread_safe_sender);
  ~WebGeofencingProviderImpl() override;

  // Enables mock geofencing service. |service_available| indicates if the
  // mock service should mock geofencing being available or not.
  void SetMockProvider(bool service_available);
  // Disables mock geofencing service.
  void ClearMockProvider();
  // Set the mock geofencing position.
  void SetMockPosition(double latitude, double longitude);

 private:
  // WebGeofencingProvider implementation.
  void registerRegion(
      const blink::WebString& regionId,
      const blink::WebCircularGeofencingRegion& region,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebGeofencingCallbacks* callbacks) override;
  void unregisterRegion(
      const blink::WebString& regionId,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebGeofencingCallbacks* callbacks) override;
  void getRegisteredRegions(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebGeofencingRegionsCallbacks* callbacks) override;

  GeofencingDispatcher* GetDispatcher();

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(WebGeofencingProviderImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_GEOFENCING_WEB_GEOFENCING_PROVIDER_IMPL_H_
