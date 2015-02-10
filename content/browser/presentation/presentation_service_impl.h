// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_

#include "base/macros.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

// Implements the PresentationService Mojo interface.
// This service can be created from a RenderFrameHost.
class PresentationServiceImpl :
    public mojo::InterfaceImpl<presentation::PresentationService> {
 public:
  ~PresentationServiceImpl() override;

  static void CreateMojoService(
      mojo::InterfaceRequest<presentation::PresentationService> request);

 protected:
  PresentationServiceImpl();

 private:
  typedef mojo::Callback<void(bool)> AvailabilityCallback;

  // PresentationService implementation.
  void GetScreenAvailability(
        const mojo::String& presentation_url,
        const AvailabilityCallback& callback) override;
  void OnScreenAvailabilityListenerRemoved() override;

  DISALLOW_COPY_AND_ASSIGN(PresentationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
