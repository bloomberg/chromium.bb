// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include "base/logging.h"

namespace content {

PresentationServiceImpl::PresentationServiceImpl() {
}

PresentationServiceImpl::~PresentationServiceImpl() {
}

// static
void PresentationServiceImpl::CreateMojoService(
    mojo::InterfaceRequest<presentation::PresentationService> request) {
  mojo::BindToRequest(new PresentationServiceImpl(), &request);
}

void PresentationServiceImpl::GetScreenAvailability(
    const mojo::String& presentation_url,
    const AvailabilityCallback& callback) {
  NOTIMPLEMENTED();
}

void PresentationServiceImpl::OnScreenAvailabilityListenerRemoved() {
  NOTIMPLEMENTED();
}

}  // namespace content
