// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "third_party/WebKit/public/platform/modules/geolocation/geolocation_service.mojom.h"

namespace blink {
namespace mojom {
enum class PermissionStatus;
}
}  // namespace blink

namespace content {
class RenderFrameHost;
class PermissionManager;

class GeolocationServiceImplContext {
 public:
  GeolocationServiceImplContext(PermissionManager* permission_manager_);
  ~GeolocationServiceImplContext();
  void RequestPermission(
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback);

 private:
  PermissionManager* permission_manager_;
  int request_id_;

  void HandlePermissionStatus(
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback,
      blink::mojom::PermissionStatus permission_status);

  base::WeakPtrFactory<GeolocationServiceImplContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationServiceImplContext);
};

class CONTENT_EXPORT GeolocationServiceImpl
    : public blink::mojom::GeolocationService {
 public:
  GeolocationServiceImpl(device::mojom::GeolocationContext* geolocation_context,
                         PermissionManager* permission_manager,
                         RenderFrameHost* render_frame_host);
  ~GeolocationServiceImpl() override;

  // Binds to the GeolocationService.
  void Bind(blink::mojom::GeolocationServiceRequest request);

  // Creates a Geolocation instance.
  // This may not be called a second time until the Geolocation instance has
  // been created.
  void CreateGeolocation(device::mojom::GeolocationRequest request,
                         bool user_gesture) override;

 private:
  // Creates the Geolocation Service.
  void CreateGeolocationWithPermissionStatus(
      device::mojom::GeolocationRequest request,
      blink::mojom::PermissionStatus permission_status);

  device::mojom::GeolocationContext* geolocation_context_;
  PermissionManager* permission_manager_;
  RenderFrameHost* render_frame_host_;

  // Along with each GeolocationService, we store a
  // GeolocationServiceImplContext which primarily exists to manage a
  // Permission Request ID.
  mojo::BindingSet<blink::mojom::GeolocationService,
                   std::unique_ptr<GeolocationServiceImplContext>>
      binding_set_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_
