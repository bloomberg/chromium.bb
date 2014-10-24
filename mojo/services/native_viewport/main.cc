// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/native_viewport/gpu_impl.h"
#include "mojo/services/native_viewport/native_viewport_impl.h"
#include "mojo/services/public/cpp/native_viewport/args.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace mojo {

class NativeViewportAppDelegate
    : public ApplicationDelegate,
      public InterfaceFactory<NativeViewport>,
      public InterfaceFactory<Gpu> {
 public:
  NativeViewportAppDelegate()
      : share_group_(new gfx::GLShareGroup),
        mailbox_manager_(new gpu::gles2::MailboxManagerImpl),
        is_headless_(false) {}
  ~NativeViewportAppDelegate() override {}

 private:
  bool HasArg(const std::string& arg) {
    const auto& args = app_->args();
    return std::find(args.begin(), args.end(), arg) != args.end();
  }

  // ApplicationDelegate implementation.
  void Initialize(ApplicationImpl* application) override {
    app_ = application;

#if !defined(COMPONENT_BUILD)
    if (HasArg(kUseTestConfig))
      gfx::GLSurface::InitializeOneOffForTests();
    else
      gfx::GLSurface::InitializeOneOff();
#endif
    is_headless_ = HasArg(kUseHeadlessConfig);
  }

  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<NativeViewport>(this);
    connection->AddService<Gpu>(this);
    return true;
  }

  // InterfaceFactory<NativeViewport> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<NativeViewport> request) override {
    BindToRequest(new NativeViewportImpl(app_, is_headless_), &request);
  }

  // InterfaceFactory<Gpu> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<Gpu> request) override {
    BindToRequest(new GpuImpl(share_group_.get(), mailbox_manager_.get()),
                  &request);
  }

  ApplicationImpl* app_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  bool is_headless_;
  DISALLOW_COPY_AND_ASSIGN(NativeViewportAppDelegate);
};
}

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::NativeViewportAppDelegate);
  runner.set_message_loop_type(base::MessageLoop::TYPE_UI);
  return runner.Run(shell_handle);
}
