// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/host/child_process.h"

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
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/runner/child/child_controller.mojom.h"
#include "mojo/runner/host/native_application_support.h"
#include "mojo/runner/host/switches.h"
#include "mojo/runner/init.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_channel_pair.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"
#include "third_party/mojo/src/mojo/edk/embedder/scoped_platform_handle.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "mojo/runner/host/linux_sandbox.h"
#endif

namespace mojo {
namespace runner {

namespace {

void DidCreateChannel(embedder::ChannelInfo* channel_info) {
  DVLOG(2) << "ChildControllerImpl::DidCreateChannel()";
}

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
// TODO(use_chrome_edk)
// class AppContext : public edk::ProcessDelegate {
class AppContext : public embedder::ProcessDelegate {
 public:
  AppContext()
      : io_thread_("io_thread"), controller_thread_("controller_thread") {}
  ~AppContext() override {}

  void Init() {
    embedder::PreInitializeChildProcess();
    // Initialize Mojo before starting any threads.
    embedder::Init();

    // Create and start our I/O thread.
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(io_thread_options));
    io_runner_ = io_thread_.task_runner().get();
    CHECK(io_runner_.get());

    // TODO(vtl): This should be SLAVE, not NONE.
    // This must be created before controller_thread_ since MessagePumpMojo will
    // create a message pipe which requires this code to be run first.
    embedder::InitIPCSupport(embedder::ProcessType::NONE, this, io_runner_,
                             embedder::ScopedPlatformHandle());
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
    embedder::ShutdownIPCSupport();
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

class ChildControllerImpl : public ChildController {
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
  void StartApp(InterfaceRequest<Application> application_request,
                const StartAppCallback& on_app_complete) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    on_app_complete_ = on_app_complete;
    unblocker_.Unblock(base::Bind(&ChildControllerImpl::StartAppOnMainThread,
                                  base::Unretained(app_library_),
                                  base::Passed(&application_request)));
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
      InterfaceRequest<Application> application_request) {
    if (!RunNativeApplication(app_library, std::move(application_request))) {
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
scoped_ptr<mojo::runner::LinuxSandbox> InitializeSandbox() {
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
  scoped_ptr<mojo::runner::LinuxSandbox> sandbox(
      new mojo::runner::LinuxSandbox(permissions));
  sandbox->Warmup();
  sandbox->EngageNamespaceSandbox();
  sandbox->EngageSeccompSandbox();
  sandbox->Seal();
  return sandbox;
}
#endif

ScopedMessagePipeHandle InitializeHostMessagePipe(
    embedder::ScopedPlatformHandle platform_channel,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  ScopedMessagePipeHandle host_message_pipe(
      embedder::CreateChannel(std::move(platform_channel),
                              base::Bind(&DidCreateChannel), io_task_runner));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch("use-new-edk")) {
    // When using the new Mojo EDK, each message pipe is backed by a platform
    // handle. The one platform handle that comes on the command line is used
    // to bind to the ChildController interface. However we also want a
    // platform handle to setup the communication channel by which we exchange
    // handles to/from tokens, which is needed for sandboxed Windows
    // processes.
    char broker_handle[10];
    MojoHandleSignalsState state;
    MojoResult rv =
        MojoWait(host_message_pipe.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                 MOJO_DEADLINE_INDEFINITE, &state);
    CHECK_EQ(MOJO_RESULT_OK, rv);
    uint32_t num_bytes = arraysize(broker_handle);
    rv = MojoReadMessage(host_message_pipe.get().value(),
                         broker_handle, &num_bytes, nullptr, 0,
                         MOJO_READ_MESSAGE_FLAG_NONE);
    CHECK_EQ(MOJO_RESULT_OK, rv);

    edk::ScopedPlatformHandle broker_channel =
        edk::PlatformChannelPair::PassClientHandleFromParentProcessFromString(
            std::string(broker_handle, num_bytes));
    CHECK(broker_channel.is_valid());
    embedder::SetParentPipeHandle(
        mojo::embedder::ScopedPlatformHandle(mojo::embedder::PlatformHandle(
            broker_channel.release().handle)));
  }

  return host_message_pipe;
}

}  // namespace

int ChildProcessMain() {
  DVLOG(2) << "ChildProcessMain()";
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  scoped_ptr<mojo::runner::LinuxSandbox> sandbox;
#endif
  base::NativeLibrary app_library = 0;
  // Load the application library before we engage the sandbox.
  app_library = mojo::runner::LoadNativeApplication(
      command_line.GetSwitchValuePath(switches::kChildProcess));
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

  embedder::ScopedPlatformHandle platform_channel =
      embedder::PlatformChannelPair::PassClientHandleFromParentProcess(
          command_line);
  CHECK(platform_channel.is_valid());

  DCHECK(!base::MessageLoop::current());

  AppContext app_context;
  app_context.Init();
  ScopedMessagePipeHandle host_message_pipe = InitializeHostMessagePipe(
      std::move(platform_channel), app_context.io_runner());
  app_context.StartControllerThread();
  Blocker blocker;
  app_context.controller_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChildControllerImpl::Init, base::Unretained(&app_context),
                 base::Unretained(app_library),
                 base::Passed(&host_message_pipe), blocker.GetUnblocker()));
  // This will block, then run whatever the controller wants.
  blocker.Block();

  app_context.Shutdown();

  return 0;
}

}  // namespace runner
}  // namespace mojo
