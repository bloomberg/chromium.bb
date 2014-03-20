// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/app_child_process.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/system/core_cpp.h"
#include "mojo/shell/app_child_process.mojom.h"

namespace mojo {
namespace shell {

namespace {

class AppChildControllerImpl;

// AppContext ------------------------------------------------------------------

// Should be created and initialized on the main thread.
class AppContext {
 public:
  AppContext()
      : io_thread_("io_thread"),
        controller_thread_("controller_thread") {}
  ~AppContext() {}

  void Init() {
    // Initialize Mojo before starting any threads.
    embedder::Init();

    // Create and start our I/O thread.
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(io_thread_options));
    io_runner_ = io_thread_.message_loop_proxy().get();
    CHECK(io_runner_);

    // Create and start our controller thread.
    base::Thread::Options controller_thread_options;
    controller_thread_options.message_loop_type =
        base::MessageLoop::TYPE_CUSTOM;
    controller_thread_options.message_pump_factory =
        base::Bind(&common::MessagePumpMojo::Create);
    CHECK(controller_thread_.StartWithOptions(controller_thread_options));
    controller_runner_ = controller_thread_.message_loop_proxy().get();
    CHECK(controller_runner_);
  }

  base::SingleThreadTaskRunner* io_runner() const {
    return io_runner_.get();
  }

  base::SingleThreadTaskRunner* controller_runner() const {
    return controller_runner_.get();
  }

  AppChildControllerImpl* controller() const {
    return controller_.get();
  }

  void set_controller(scoped_ptr<AppChildControllerImpl> controller) {
    controller_ = controller.Pass();
  }

 private:
  base::Thread io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  base::Thread controller_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> controller_runner_;

  // Accessed only on the controller thread.
  scoped_ptr<AppChildControllerImpl> controller_;

  DISALLOW_COPY_AND_ASSIGN(AppContext);
};

// AppChildControllerImpl ------------------------------------------------------

class AppChildControllerImpl : public mojo_shell::AppChildController {
 public:
  virtual ~AppChildControllerImpl() {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  // To be executed on the controller thread. Creates the |AppChildController|,
  // etc.
  static void Init(
      AppContext* app_context,
      embedder::ScopedPlatformHandle platform_channel,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner) {
    DCHECK(app_context);
    DCHECK(platform_channel.is_valid());
    DCHECK(main_thread_runner);

    DCHECK(!app_context->controller());
    app_context->set_controller(
        make_scoped_ptr(new AppChildControllerImpl(app_context)));
    app_context->controller()->CreateChannel(platform_channel.Pass(),
                                             main_thread_runner);

  }

  // |AppChildController| method:
  virtual void StartApp(const String& app_path,
                        ScopedMessagePipeHandle service) OVERRIDE {
    DVLOG(2) << "AppChildControllerImpl::StartApp("
             << app_path.To<std::string>() << ", ...)";

    // TODO(vtl): Load/run app.
  }

 private:
  AppChildControllerImpl(AppContext* app_context)
      : app_context_(app_context),
        channel_info_(NULL) {
  }

  void CreateChannel(
      embedder::ScopedPlatformHandle platform_channel,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner) {
    DVLOG(2) << "AppChildControllerImpl::CreateChannel()";
    DCHECK(thread_checker_.CalledOnValidThread());

    ScopedMessagePipeHandle host_message_pipe(embedder::CreateChannel(
        platform_channel.Pass(),
        app_context_->io_runner(),
        base::Bind(&AppChildControllerImpl::DidCreateChannel,
                   base::Unretained(this), main_thread_runner),
        base::MessageLoopProxy::current()));
    controller_client_.reset(
        mojo_shell::ScopedAppChildControllerClientHandle(
            mojo_shell::AppChildControllerClientHandle(
                host_message_pipe.release().value())), this);
  }

  // Callback for |embedder::CreateChannel()|.
  void DidCreateChannel(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      embedder::ChannelInfo* channel_info) {
    DVLOG(2) << "AppChildControllerImpl::DidCreateChannel()";
    DCHECK(thread_checker_.CalledOnValidThread());
    channel_info_ = channel_info;
  }

  base::ThreadChecker thread_checker_;
  AppContext* const app_context_;

  RemotePtr<mojo_shell::AppChildControllerClient> controller_client_;
  embedder::ChannelInfo* channel_info_;

  DISALLOW_COPY_AND_ASSIGN(AppChildControllerImpl);
};

}  // namespace

// AppChildProcess -------------------------------------------------------------

AppChildProcess::AppChildProcess() {
}

AppChildProcess::~AppChildProcess() {
}

void AppChildProcess::Main() {
  DVLOG(2) << "AppChildProcess::Main()";

  AppContext app_context;
  app_context.Init();

  {
    base::MessageLoop message_loop;

    app_context.controller_runner()->PostTask(
        FROM_HERE,
        base::Bind(&AppChildControllerImpl::Init,
                   base::Unretained(&app_context),
                   base::Passed(platform_channel()),
                   scoped_refptr<base::SingleThreadTaskRunner>(
                       message_loop.message_loop_proxy())));

    // Eventually, we'll get a task posted telling us to quit this message loop,
    // which will also tell us what to do afterwards (e.g., run |MojoMain()|).
    message_loop.Run();
  }
}

}  // namespace shell
}  // namespace mojo
