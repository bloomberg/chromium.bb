// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/native_viewport/gpu_impl.h"
#include "mojo/services/native_viewport/native_viewport_impl.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace mojo {

class NativeViewportAppDelegate
    : public ApplicationDelegate,
      public InterfaceFactory<NativeViewport>,
      public InterfaceFactory<Gpu>,
      public InterfaceFactory<NativeViewportConfig> {
 public:
  NativeViewportAppDelegate()
      : share_group_(new gfx::GLShareGroup),
        mailbox_manager_(new gpu::gles2::MailboxManager),
        is_test_(false),
        is_headless_(false),
        is_initialized_(false) {}
  virtual ~NativeViewportAppDelegate() {}

 private:
  class NativeViewportConfigImpl : public InterfaceImpl<NativeViewportConfig> {
   public:
    NativeViewportConfigImpl(NativeViewportAppDelegate* app_delegate)
      : app_delegate_(app_delegate) {}

    virtual void UseTestConfig(
        const Callback<void()>& callback) OVERRIDE {
      app_delegate_->is_test_ = true;
      callback.Run();
    }

    virtual void UseHeadlessConfig(
        const Callback<void()>& callback) OVERRIDE {
      app_delegate_->is_headless_ = true;
      callback.Run();
    }

   private:
    NativeViewportAppDelegate* app_delegate_;
  };

  // ApplicationDelegate implementation.
  virtual void Initialize(ApplicationImpl* application) OVERRIDE {
    app_ = application;
  }

  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) OVERRIDE {
    connection->AddService<NativeViewport>(this);
    connection->AddService<Gpu>(this);
    connection->AddService<NativeViewportConfig>(this);
    return true;
  }

  // InterfaceFactory<NativeViewport> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<NativeViewport> request) OVERRIDE {
#if !defined(COMPONENT_BUILD)
    if (!is_initialized_) {
      if (is_test_)
        gfx::GLSurface::InitializeOneOffForTests();
      else
        gfx::GLSurface::InitializeOneOff();
      is_initialized_ = true;
    }
#endif
    BindToRequest(new NativeViewportImpl(app_, is_headless_), &request);
  }

  // InterfaceFactory<Gpu> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<Gpu> request) OVERRIDE {
    BindToRequest(new GpuImpl(share_group_.get(), mailbox_manager_.get()),
                  &request);
  }

  // InterfaceFactory<NVTestConfig> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<NativeViewportConfig> request) OVERRIDE {
    BindToRequest(new NativeViewportConfigImpl(this), &request);
  }

  ApplicationImpl* app_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  bool is_test_;
  bool is_headless_;
  bool is_initialized_;
  DISALLOW_COPY_AND_ASSIGN(NativeViewportAppDelegate);
};
}

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::NativeViewportAppDelegate);
  runner.set_message_loop_type(base::MessageLoop::TYPE_UI);
  return runner.Run(shell_handle);
}
