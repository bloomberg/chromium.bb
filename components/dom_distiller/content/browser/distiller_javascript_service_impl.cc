// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"

namespace dom_distiller {

DistillerJavaScriptServiceImpl::DistillerJavaScriptServiceImpl(
      mojo::InterfaceRequest<DistillerJavaScriptService> request)
    : binding_(this, request.Pass()) {}

DistillerJavaScriptServiceImpl::~DistillerJavaScriptServiceImpl() {}

void DistillerJavaScriptServiceImpl::HandleDistillerEchoCall(
    const mojo::String& message) {}

void CreateDistillerJavaScriptService(
    mojo::InterfaceRequest<DistillerJavaScriptService> request) {
  // This is strongly bound and owned by the pipe.
  new DistillerJavaScriptServiceImpl(request.Pass());
}

}  // namespace dom_distiller
