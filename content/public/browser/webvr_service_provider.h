// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBVR_SERVICE_PROVIDER_H
#define CONTENT_PUBLIC_BROWSER_WEBVR_SERVICE_PROVIDER_H

#include "base/callback.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace mojom {
class VRService;
}
}  // namespace device

namespace content {

// Provides access to the browser process representation of a WebVR site
// session. See VrServiceImpl.
class WebvrServiceProvider {
 public:
  static void BindWebvrService(
      int render_frame_process_id,
      int render_frame_routing_id,
      mojo::InterfaceRequest<device::mojom::VRService> request);

  using BindWebvrServiceCallback = base::Callback<
      void(int, int, mojo::InterfaceRequest<device::mojom::VRService>)>;

  CONTENT_EXPORT static void SetWebvrServiceCallback(
      BindWebvrServiceCallback callback);

 private:
  WebvrServiceProvider() = default;
  ~WebvrServiceProvider() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBVR_SERVICE_PROVIDER_H
