// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/runner/child/runner_connection.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/runner/child/child_controller.mojom.h"
#include "mojo/shell/runner/common/switches.h"

namespace mojo {
namespace shell {
namespace {

// Blocks a thread until another thread unblocks it, at which point it unblocks
// and runs a closure provided by that thread.
class Blocker {
 public:
  class Unblocker {
   public:
    explicit Unblocker(Blocker* blocker = nullptr) : blocker_(blocker) {}
    ~Unblocker() {}

    void Unblock(base::Closure run_after) {
      DCHECK(blocker_);
      DCHECK(blocker_->run_after_.is_null());
      blocker_->run_after_ = run_after;
      blocker_->event_.Signal();
      blocker_ = nullptr;
    }

   private:
    Blocker* blocker_;

    // Copy and assign allowed.
  };

  Blocker() : event_(true, false) {}
  ~Blocker() {}

  void Block() {
    DCHECK(run_after_.is_null());
    event_.Wait();
    if (!run_after_.is_null())
      run_after_.Run();
  }

  Unblocker GetUnblocker() { return Unblocker(this); }

 private:
  base::WaitableEvent event_;
  base::Closure run_after_;

  DISALLOW_COPY_AND_ASSIGN(Blocker);
};

using GotApplicationRequestCallback =
    base::Callback<void(InterfaceRequest<Application>)>;

void OnCreateMessagePipe(ScopedMessagePipeHandle* result,
                         Blocker::Unblocker unblocker,
                         ScopedMessagePipeHandle pipe) {
  *result = std::move(pipe);
  unblocker.Unblock(base::Bind(&base::DoNothing));
}

void OnGotApplicationRequest(InterfaceRequest<Application>* out_request,
                             InterfaceRequest<Application> request) {
  *out_request = std::move(request);
}

class ChildControllerImpl;

class RunnerConnectionImpl : public RunnerConnection {
 public:
  RunnerConnectionImpl() : controller_thread_("controller_thread") {
    StartControllerThread();
  }
  ~RunnerConnectionImpl() override {
    controller_runner_->PostTask(
        FROM_HERE, base::Bind(&RunnerConnectionImpl::ShutdownOnControllerThread,
                              base::Unretained(this)));
    controller_thread_.Stop();
  }

  // Returns true if a connection to the runner has been established and
  // |request| has been modified, false if no connection was established.
  bool WaitForApplicationRequest(InterfaceRequest<Application>* request,
                                 ScopedMessagePipeHandle handle);

  ChildControllerImpl* controller() const { return controller_.get(); }

  void set_controller(scoped_ptr<ChildControllerImpl> controller) {
    controller_ = std::move(controller);
  }

 private:
  void StartControllerThread() {
    base::Thread::Options controller_thread_options;
    controller_thread_options.message_loop_type =
        base::MessageLoop::TYPE_CUSTOM;
    controller_thread_options.message_pump_factory =
        base::Bind(&common::MessagePumpMojo::Create);
    CHECK(controller_thread_.StartWithOptions(controller_thread_options));
    controller_runner_ = controller_thread_.task_runner().get();
    CHECK(controller_runner_.get());
  }

  void ShutdownOnControllerThread() { controller_.reset(); }

  base::Thread controller_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> controller_runner_;

  // Accessed only on the controller thread.
  scoped_ptr<ChildControllerImpl> controller_;

  DISALLOW_COPY_AND_ASSIGN(RunnerConnectionImpl);
};

class ChildControllerImpl : public ChildController {
 public:
  ~ChildControllerImpl() override {
    DCHECK(thread_checker_.CalledOnValidThread());

    // TODO(vtl): Pass in the result from |MainMain()|.
    on_app_complete_.Run(MOJO_RESULT_UNIMPLEMENTED);
  }

  // To be executed on the controller thread. Creates the |ChildController|,
  // etc.
  static void Create(RunnerConnectionImpl* connection,
                     const GotApplicationRequestCallback& callback,
                     ScopedMessagePipeHandle runner_handle,
                     const Blocker::Unblocker& unblocker) {
    DCHECK(connection);
    DCHECK(!connection->controller());

    scoped_ptr<ChildControllerImpl> impl(
        new ChildControllerImpl(connection, callback, unblocker));

    impl->Bind(std::move(runner_handle));

    connection->set_controller(std::move(impl));
  }

  void Bind(ScopedMessagePipeHandle handle) {
    binding_.Bind(std::move(handle));
    binding_.set_connection_error_handler([this]() { OnConnectionError(); });
  }

  void OnConnectionError() {
    // A connection error means the connection to the shell is lost. This is not
    // recoverable.
    DLOG(ERROR) << "Connection error to the shell.";
    _exit(1);
  }

  // |ChildController| methods:
  void StartApp(InterfaceRequest<Application> application_request,
                const StartAppCallback& on_app_complete) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    on_app_complete_ = on_app_complete;
    unblocker_.Unblock(
        base::Bind(&ChildControllerImpl::ReturnApplicationRequestOnMainThread,
                   callback_, base::Passed(&application_request)));
  }

  void ExitNow(int32_t exit_code) override {
    DVLOG(2) << "ChildControllerImpl::ExitNow(" << exit_code << ")";
    _exit(exit_code);
  }

 private:
  ChildControllerImpl(RunnerConnectionImpl* connection,
                      const GotApplicationRequestCallback& callback,
                      const Blocker::Unblocker& unblocker)
      : connection_(connection),
        callback_(callback),
        unblocker_(unblocker),
        binding_(this) {}

  static void ReturnApplicationRequestOnMainThread(
      const GotApplicationRequestCallback& callback,
      InterfaceRequest<Application> application_request) {
    callback.Run(std::move(application_request));
  }

  base::ThreadChecker thread_checker_;
  RunnerConnectionImpl* const connection_;
  GotApplicationRequestCallback callback_;
  Blocker::Unblocker unblocker_;
  StartAppCallback on_app_complete_;

  Binding<ChildController> binding_;

  DISALLOW_COPY_AND_ASSIGN(ChildControllerImpl);
};

bool RunnerConnectionImpl::WaitForApplicationRequest(
    InterfaceRequest<Application>* request,
    ScopedMessagePipeHandle handle) {
  // If a valid message pipe to the runner was not provided, look for one on the
  // command line.
  if (!handle.is_valid()) {
    edk::ScopedPlatformHandle platform_channel =
        edk::PlatformChannelPair::PassClientHandleFromParentProcess(
            *base::CommandLine::ForCurrentProcess());
    if (!platform_channel.is_valid())
      return false;
    edk::SetParentPipeHandle(std::move(platform_channel));
    std::string primordial_pipe_token =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kPrimordialPipeToken);
    Blocker blocker;
    edk::CreateChildMessagePipe(
        primordial_pipe_token,
        base::Bind(&OnCreateMessagePipe, base::Unretained(&handle),
                   blocker.GetUnblocker()));
    blocker.Block();
  }

  Blocker blocker;
  controller_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &ChildControllerImpl::Create, base::Unretained(this),
          base::Bind(&OnGotApplicationRequest, base::Unretained(request)),
          base::Passed(&handle), blocker.GetUnblocker()));
  blocker.Block();

  return true;
}

}  // namespace

RunnerConnection::~RunnerConnection() {}

// static
RunnerConnection* RunnerConnection::ConnectToRunner(
    InterfaceRequest<Application>* request,
    ScopedMessagePipeHandle handle) {
  RunnerConnectionImpl* connection = new RunnerConnectionImpl;
  if (!connection->WaitForApplicationRequest(request, std::move(handle))) {
    delete connection;
    return nullptr;
  }
  return connection;
}

RunnerConnection::RunnerConnection() {}

}  // namespace shell
}  // namespace mojo
