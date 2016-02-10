// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/runner/host/child_process.h"

#include <stdint.h>

#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/runner/child/child_controller.mojom.h"
#include "mojo/shell/runner/common/switches.h"
#include "mojo/shell/runner/host/native_application_support.h"
#include "mojo/shell/runner/init.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "mojo/shell/runner/host/linux_sandbox.h"
#endif

namespace mojo {
namespace shell {

namespace {

// Blocker ---------------------------------------------------------------------

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

// AppContext ------------------------------------------------------------------

class ChildControllerImpl;

// Should be created and initialized on the main thread.
class AppContext : public edk::ProcessDelegate {
 public:
  AppContext()
      : io_thread_("io_thread"), controller_thread_("controller_thread") {}
  ~AppContext() override {}

  void Init() {
    // Initialize Mojo before starting any threads.
    edk::Init();

    // Create and start our I/O thread.
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(io_thread_options));
    io_runner_ = io_thread_.task_runner().get();
    CHECK(io_runner_.get());

    // TODO(vtl): This should be SLAVE, not NONE.
    // This must be created before controller_thread_ since MessagePumpMojo will
    // create a message pipe which requires this code to be run first.
    edk::InitIPCSupport(this, io_runner_);
  }

  void StartControllerThread() {
    // Create and start our controller thread.
    base::Thread::Options controller_thread_options;
    controller_thread_options.message_loop_type =
        base::MessageLoop::TYPE_CUSTOM;
    controller_thread_options.message_pump_factory =
        base::Bind(&common::MessagePumpMojo::Create);
    CHECK(controller_thread_.StartWithOptions(controller_thread_options));
    controller_runner_ = controller_thread_.task_runner().get();
    CHECK(controller_runner_.get());
  }

  void Shutdown() {
    Blocker blocker;
    shutdown_unblocker_ = blocker.GetUnblocker();
    controller_runner_->PostTask(
        FROM_HERE, base::Bind(&AppContext::ShutdownOnControllerThread,
                              base::Unretained(this)));
    blocker.Block();
  }

  base::SingleThreadTaskRunner* io_runner() const { return io_runner_.get(); }

  base::SingleThreadTaskRunner* controller_runner() const {
    return controller_runner_.get();
  }

  ChildControllerImpl* controller() const { return controller_.get(); }

  void set_controller(scoped_ptr<ChildControllerImpl> controller) {
    controller_ = std::move(controller);
  }

 private:
  void ShutdownOnControllerThread() {
    // First, destroy the controller.
    controller_.reset();

    // Next shutdown IPC. We'll unblock the main thread in OnShutdownComplete().
    edk::ShutdownIPCSupport();
  }

  // ProcessDelegate implementation.
  void OnShutdownComplete() override {
    shutdown_unblocker_.Unblock(base::Closure());
  }

  base::Thread io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  base::Thread controller_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> controller_runner_;

  // Accessed only on the controller thread.
  scoped_ptr<ChildControllerImpl> controller_;

  // Used to unblock the main thread on shutdown.
  Blocker::Unblocker shutdown_unblocker_;

  DISALLOW_COPY_AND_ASSIGN(AppContext);
};

// ChildControllerImpl ------------------------------------------------------

class ChildControllerImpl : public mojom::ChildController {
 public:
  ~ChildControllerImpl() override {
    DCHECK(thread_checker_.CalledOnValidThread());

    // TODO(vtl): Pass in the result from |MainMain()|.
    on_app_complete_.Run(MOJO_RESULT_UNIMPLEMENTED);
  }

  // To be executed on the controller thread. Creates the |ChildController|,
  // etc.
  static void Init(AppContext* app_context,
                   base::NativeLibrary app_library,
                   ScopedMessagePipeHandle host_message_pipe,
                   const Blocker::Unblocker& unblocker) {
    DCHECK(app_context);
    DCHECK(host_message_pipe.is_valid());

    DCHECK(!app_context->controller());

    scoped_ptr<ChildControllerImpl> impl(
        new ChildControllerImpl(app_context, app_library, unblocker));

    impl->Bind(std::move(host_message_pipe));

    app_context->set_controller(std::move(impl));
  }

  void Bind(ScopedMessagePipeHandle handle) {
    binding_.Bind(std::move(handle));
    binding_.set_connection_error_handler([this]() { OnConnectionError(); });
  }

  void OnConnectionError() {
    // A connection error means the connection to the shell is lost. This is not
    // recoverable.
    LOG(ERROR) << "Connection error to the shell.";
    _exit(1);
  }

  // |ChildController| methods:
  void StartApp(InterfaceRequest<mojom::ShellClient> request,
                const StartAppCallback& on_app_complete) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    on_app_complete_ = on_app_complete;
    unblocker_.Unblock(base::Bind(&ChildControllerImpl::StartAppOnMainThread,
                                  base::Unretained(app_library_),
                                  base::Passed(&request)));
  }

  void ExitNow(int32_t exit_code) override {
    DVLOG(2) << "ChildControllerImpl::ExitNow(" << exit_code << ")";
    _exit(exit_code);
  }

 private:
  ChildControllerImpl(AppContext* app_context,
                      base::NativeLibrary app_library,
                      const Blocker::Unblocker& unblocker)
      : app_library_(app_library), unblocker_(unblocker), binding_(this) {}

  static void StartAppOnMainThread(
      base::NativeLibrary app_library,
      InterfaceRequest<mojom::ShellClient> request) {
    if (!RunNativeApplication(app_library, std::move(request))) {
      LOG(ERROR) << "Failure to RunNativeApplication()";
    }
  }

  base::ThreadChecker thread_checker_;
  base::NativeLibrary app_library_;
  Blocker::Unblocker unblocker_;
  StartAppCallback on_app_complete_;

  Binding<ChildController> binding_;

  DISALLOW_COPY_AND_ASSIGN(ChildControllerImpl);
};

#if defined(OS_LINUX) && !defined(OS_ANDROID)
scoped_ptr<mojo::shell::LinuxSandbox> InitializeSandbox() {
  using sandbox::syscall_broker::BrokerFilePermission;
  // Warm parts of base in the copy of base in the mojo runner.
  base::RandUint64();
  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::MaxSharedMemorySize();
  base::SysInfo::NumberOfProcessors();

  // TODO(erg,jln): Allowing access to all of /dev/shm/ makes it easy to
  // spy on other shared memory using processes. This is a temporary hack
  // so that we have some sandbox until we have proper shared memory
  // support integrated into mojo.
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(
      BrokerFilePermission::ReadWriteCreateUnlinkRecursive("/dev/shm/"));
  scoped_ptr<mojo::shell::LinuxSandbox> sandbox(
      new mojo::shell::LinuxSandbox(permissions));
  sandbox->Warmup();
  sandbox->EngageNamespaceSandbox();
  sandbox->EngageSeccompSandbox();
  sandbox->Seal();
  return sandbox;
}
#endif

ScopedMessagePipeHandle InitializeHostMessagePipe(
    edk::ScopedPlatformHandle platform_channel,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  edk::SetParentPipeHandle(std::move(platform_channel));
  std::string primordial_pipe_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPrimordialPipeToken);
  return edk::CreateChildMessagePipe(primordial_pipe_token);
}

}  // namespace

int ChildProcessMain() {
  DVLOG(2) << "ChildProcessMain()";
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  scoped_ptr<mojo::shell::LinuxSandbox> sandbox;
#endif
  base::NativeLibrary app_library = 0;
  // Load the application library before we engage the sandbox.
  base::FilePath app_library_path =
      command_line.GetSwitchValuePath(switches::kChildProcess);
  if (!app_library_path.empty())
    app_library = mojo::shell::LoadNativeApplication(app_library_path);
  base::i18n::InitializeICU();
  if (app_library)
    CallLibraryEarlyInitialization(app_library);

#if !defined(OFFICIAL_BUILD)
  // Initialize stack dumping just before initializing sandbox to make
  // sure symbol names in all loaded libraries will be cached.
  base::debug::EnableInProcessStackDumping();
#endif
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kEnableSandbox))
    sandbox = InitializeSandbox();
#endif

  edk::ScopedPlatformHandle platform_channel =
      edk::PlatformChannelPair::PassClientHandleFromParentProcess(command_line);
  CHECK(platform_channel.is_valid());

  DCHECK(!base::MessageLoop::current());

  Blocker blocker;
  AppContext app_context;
  app_context.Init();
  app_context.StartControllerThread();

  ScopedMessagePipeHandle host_pipe = InitializeHostMessagePipe(
      std::move(platform_channel), app_context.io_runner());
  app_context.controller_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChildControllerImpl::Init, &app_context, app_library,
                 base::Passed(&host_pipe), blocker.GetUnblocker()));

  // This will block, then run whatever the controller wants.
  blocker.Block();

  app_context.Shutdown();

  return 0;
}

}  // namespace shell
}  // namespace mojo
