// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/android/native_viewport_application_loader.h"

#include "components/gles2/gpu_state.h"
#include "components/native_viewport/native_viewport_impl.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace mojo {
namespace runner {

NativeViewportApplicationLoader::NativeViewportApplicationLoader() {
}

NativeViewportApplicationLoader::~NativeViewportApplicationLoader() {
}

void NativeViewportApplicationLoader::Load(
    const GURL& url,
    InterfaceRequest<Application> application_request) {
  DCHECK(application_request.is_pending());
  app_.reset(new ApplicationImpl(this, application_request.Pass()));
}

bool NativeViewportApplicationLoader::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<NativeViewport>(this);
  connection->AddService<Gpu>(this);
  return true;
}

void NativeViewportApplicationLoader::Create(
    ApplicationConnection* connection,
    InterfaceRequest<NativeViewport> request) {
  if (!gpu_state_)
    gpu_state_ = new gles2::GpuState;
  // Pass a null AppRefCount because on Android the NativeViewPort app must
  // live on the main thread and we don't want to exit that when all the native
  // viewports are gone.
  new native_viewport::NativeViewportImpl(
      false, gpu_state_, request.Pass(),
      make_scoped_ptr<mojo::AppRefCount>(nullptr));
}

void NativeViewportApplicationLoader::Create(ApplicationConnection* connection,
                                             InterfaceRequest<Gpu> request) {
  if (!gpu_state_)
    gpu_state_ = new gles2::GpuState;
  new gles2::GpuImpl(request.Pass(), gpu_state_);
}

}  // namespace runner
}  // namespace mojo
