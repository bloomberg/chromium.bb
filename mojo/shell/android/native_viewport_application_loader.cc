// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/android/native_viewport_application_loader.h"

#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/gles2/gpu_state.h"
#include "mojo/services/native_viewport/native_viewport_impl.h"
#include "mojo/shell/android/keyboard_impl.h"

namespace mojo {
namespace shell {

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
  connection->AddService<Keyboard>(this);
  return true;
}

void NativeViewportApplicationLoader::Create(
    ApplicationConnection* connection,
    InterfaceRequest<NativeViewport> request) {
  if (!gpu_state_)
    gpu_state_ = new gles2::GpuState;
  new native_viewport::NativeViewportImpl(false, gpu_state_, request.Pass());
}

void NativeViewportApplicationLoader::Create(
    ApplicationConnection* connection,
    InterfaceRequest<Keyboard> request) {
  new KeyboardImpl(request.Pass());
}

void NativeViewportApplicationLoader::Create(ApplicationConnection* connection,
                                             InterfaceRequest<Gpu> request) {
  if (!gpu_state_)
    gpu_state_ = new gles2::GpuState;
  new gles2::GpuImpl(request.Pass(), gpu_state_);
}

}  // namespace shell
}  // namespace mojo
