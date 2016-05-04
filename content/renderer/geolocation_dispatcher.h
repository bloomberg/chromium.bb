// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_
#define CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_

#include <memory>

#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/geolocation/geolocation.mojom.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom.h"
#include "third_party/WebKit/public/web/WebGeolocationClient.h"
#include "third_party/WebKit/public/web/WebGeolocationController.h"

namespace blink {
class WebGeolocationController;
class WebGeolocationPermissionRequest;
class WebGeolocationPermissionRequestManager;
class WebGeolocationPosition;
}

namespace content {
struct Geoposition;

// GeolocationDispatcher is a delegate for Geolocation messages used by
// WebKit.
// It's the complement of GeolocationDispatcherHost.
class GeolocationDispatcher
    : public RenderFrameObserver,
      public blink::WebGeolocationClient {
 public:
  explicit GeolocationDispatcher(RenderFrame* render_frame);
  ~GeolocationDispatcher() override;

 private:
  // WebGeolocationClient
  void startUpdating() override;
  void stopUpdating() override;
  void setEnableHighAccuracy(bool enable_high_accuracy) override;
  void setController(blink::WebGeolocationController* controller) override;
  bool lastPosition(blink::WebGeolocationPosition& position) override;
  void requestPermission(
      const blink::WebGeolocationPermissionRequest& permissionRequest) override;
  void cancelPermissionRequest(
      const blink::WebGeolocationPermissionRequest& permissionRequest) override;

  void QueryNextPosition();
  void OnPositionUpdate(blink::mojom::GeopositionPtr geoposition);

  // Permission for using geolocation has been set.
  void OnPermissionSet(int permission_request_id,
                       blink::mojom::PermissionStatus status);

  std::unique_ptr<blink::WebGeolocationController> controller_;

  std::unique_ptr<blink::WebGeolocationPermissionRequestManager>
      pending_permissions_;
  blink::mojom::GeolocationServicePtr geolocation_service_;
  bool enable_high_accuracy_;
  blink::mojom::PermissionServicePtr permission_service_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_
