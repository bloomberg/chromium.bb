// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_ANDROID_NATIVE_VIEWPORT_APPLICATION_LOADER_H_
#define MOJO_RUNNER_ANDROID_NATIVE_VIEWPORT_APPLICATION_LOADER_H_

#include "components/gles2/gpu_impl.h"
#include "components/gpu/public/interfaces/gpu.mojom.h"
#include "components/native_viewport/public/interfaces/native_viewport.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/shell/application_loader.h"

namespace gles2 {
class GpuState;
}

namespace mojo {

class ApplicationImpl;

namespace runner {

class NativeViewportApplicationLoader : public shell::ApplicationLoader,
                                        public ApplicationDelegate,
                                        public InterfaceFactory<NativeViewport>,
                                        public InterfaceFactory<Gpu> {
 public:
  NativeViewportApplicationLoader();
  ~NativeViewportApplicationLoader();

 private:
  // ApplicationLoader implementation.
  void Load(const GURL& url,
            InterfaceRequest<Application> application_request) override;

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override;

  // InterfaceFactory<NativeViewport> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<NativeViewport> request) override;

  // InterfaceFactory<Gpu> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<Gpu> request) override;

  scoped_refptr<gles2::GpuState> gpu_state_;
  scoped_ptr<ApplicationImpl> app_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportApplicationLoader);
};

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_ANDROID_NATIVE_VIEWPORT_APPLICATION_LOADER_H_
