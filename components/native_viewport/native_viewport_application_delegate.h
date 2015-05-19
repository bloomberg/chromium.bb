// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NATIVE_VIEWPORT_NATIVE_VIEWPORT_APPLICATION_DELEGATE_H_
#define COMPONENTS_NATIVE_VIEWPORT_NATIVE_VIEWPORT_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "components/gles2/gpu_impl.h"
#include "components/native_viewport/public/interfaces/native_viewport.mojom.h"
#include "mojo/application/app_lifetime_helper.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/common/tracing_impl.h"

namespace mojo {
class ApplicationConnection;
class ApplicationImpl;
}

namespace ui {
class PlatformEventSource;
}

namespace native_viewport {

class NativeViewportApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::NativeViewport>,
      public mojo::InterfaceFactory<mojo::Gpu> {
 public:
  NativeViewportApplicationDelegate();
  ~NativeViewportApplicationDelegate() override;

 private:
  // mojo::ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* application) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mojo::InterfaceFactory<NativeViewport> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NativeViewport> request) override;

  // mojo::InterfaceFactory<Gpu> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::Gpu> request) override;

  scoped_refptr<gles2::GpuState> gpu_state_;
  scoped_ptr<ui::PlatformEventSource> event_source_;
  bool is_headless_;
  mojo::TracingImpl tracing_;
  mojo::AppLifetimeHelper app_lifetime_helper_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportApplicationDelegate);
};

}  // namespace native_viewport

#endif  // COMPONENTS_NATIVE_VIEWPORT_NATIVE_VIEWPORT_APPLICATION_DELEGATE_H_
