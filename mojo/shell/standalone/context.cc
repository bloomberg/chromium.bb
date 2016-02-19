// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/standalone/context.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/process_info.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/tracing_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/tracing/public/cpp/switches.h"
#include "mojo/services/tracing/public/cpp/trace_provider_impl.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/runner/host/command_line_switch.h"
#include "mojo/shell/runner/host/in_process_native_runner.h"
#include "mojo/shell/runner/host/out_of_process_native_runner.h"
#include "mojo/shell/standalone/switches.h"
#include "mojo/shell/standalone/tracer.h"
#include "mojo/shell/switches.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
namespace {

// Used to ensure we only init once.
class Setup {
 public:
  Setup() { edk::Init(); }

  ~Setup() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

class TracingInterfaceProvider : public shell::mojom::InterfaceProvider {
 public:
  TracingInterfaceProvider(Tracer* tracer,
                           shell::mojom::InterfaceProviderRequest request)
      : tracer_(tracer), binding_(this, std::move(request)) {}
  ~TracingInterfaceProvider() override {}

  // shell::mojom::InterfaceProvider:
  void GetInterface(const mojo::String& interface_name,
                    ScopedMessagePipeHandle client_handle) override {
    if (tracer_ && interface_name == tracing::TraceProvider::Name_) {
      tracer_->ConnectToProvider(
          MakeRequest<tracing::TraceProvider>(std::move(client_handle)));
    }
  }

 private:
  Tracer* tracer_;
  StrongBinding<shell::mojom::InterfaceProvider> binding_;

  DISALLOW_COPY_AND_ASSIGN(TracingInterfaceProvider);
};

}  // namespace

Context::Context() : main_entry_time_(base::Time::Now()) {}

Context::~Context() {
  DCHECK(!base::MessageLoop::current());
}

// static
void Context::EnsureEmbedderIsInitialized() {
  static base::LazyInstance<Setup>::Leaky setup = LAZY_INSTANCE_INITIALIZER;
  setup.Get();
}

void Context::Init(const base::FilePath& shell_file_root) {
  TRACE_EVENT0("mojo_shell", "Context::Init");
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  bool trace_startup = command_line.HasSwitch(::switches::kTraceStartup);
  if (trace_startup) {
    tracer_.Start(
        command_line.GetSwitchValueASCII(::switches::kTraceStartup),
        command_line.GetSwitchValueASCII(::switches::kTraceStartupDuration),
        "mojo_runner.trace");
  }

  EnsureEmbedderIsInitialized();
  task_runners_.reset(
      new TaskRunners(base::MessageLoop::current()->task_runner()));

  // TODO(vtl): This should be MASTER, not NONE.
  edk::InitIPCSupport(this, task_runners_->io_runner());

  scoped_ptr<NativeRunnerFactory> runner_factory;
  if (command_line.HasSwitch(switches::kSingleProcess)) {
#if defined(COMPONENT_BUILD)
    LOG(ERROR) << "Running Mojo in single process component build, which isn't "
               << "supported because statics in apps interact. Use static build"
               << " or don't pass --single-process.";
#endif
    runner_factory.reset(
        new InProcessNativeRunnerFactory(task_runners_->blocking_pool()));
  } else {
    runner_factory.reset(new OutOfProcessNativeRunnerFactory(
        task_runners_->blocking_pool(), command_line_switches_));
  }
  application_manager_.reset(new ApplicationManager(
      std::move(runner_factory), task_runners_->blocking_pool(), true));

  shell::mojom::InterfaceProviderPtr tracing_remote_interfaces;
  shell::mojom::InterfaceProviderPtr tracing_local_interfaces;
  new TracingInterfaceProvider(&tracer_, GetProxy(&tracing_local_interfaces));

  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->set_source(Identity(GURL("mojo:shell"), std::string(),
                              GetPermissiveCapabilityFilter()));
  params->set_target(Identity(GURL("mojo:tracing"), std::string(),
                              GetPermissiveCapabilityFilter()));
  params->set_remote_interfaces(GetProxy(&tracing_remote_interfaces));
  params->set_local_interfaces(std::move(tracing_local_interfaces));
  application_manager_->ConnectToApplication(std::move(params));

  if (command_line.HasSwitch(tracing::kTraceStartup)) {
    tracing::TraceCollectorPtr coordinator;
    auto coordinator_request = GetProxy(&coordinator);
    tracing_remote_interfaces->GetInterface(
        tracing::TraceCollector::Name_, coordinator_request.PassMessagePipe());
    tracer_.StartCollectingFromTracingService(std::move(coordinator));
  }

  // Record the shell startup metrics used for performance testing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          tracing::kEnableStatsCollectionBindings)) {
    tracing::StartupPerformanceDataCollectorPtr collector;
    tracing_remote_interfaces->GetInterface(
        tracing::StartupPerformanceDataCollector::Name_,
        GetProxy(&collector).PassMessagePipe());
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
    // CurrentProcessInfo::CreationTime is only defined on some platforms.
    const base::Time creation_time = base::CurrentProcessInfo::CreationTime();
    collector->SetShellProcessCreationTime(creation_time.ToInternalValue());
#endif
    collector->SetShellMainEntryPointTime(main_entry_time_.ToInternalValue());
  }
}

void Context::Shutdown() {
  // Actions triggered by ApplicationManager's destructor may require a current
  // message loop, so we should destruct it explicitly now as ~Context() occurs
  // post message loop shutdown.
  application_manager_.reset();

  TRACE_EVENT0("mojo_shell", "Context::Shutdown");
  DCHECK_EQ(base::MessageLoop::current()->task_runner(),
            task_runners_->shell_runner());
  // Post a task in case OnShutdownComplete is called synchronously.
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(edk::ShutdownIPCSupport));
  // We'll quit when we get OnShutdownComplete().
  base::MessageLoop::current()->Run();
}

void Context::OnShutdownComplete() {
  DCHECK_EQ(base::MessageLoop::current()->task_runner(),
            task_runners_->shell_runner());
  base::MessageLoop::current()->QuitWhenIdle();
}

void Context::Run(const GURL& url) {
  DCHECK(app_complete_callback_.is_null());
  shell::mojom::InterfaceProviderPtr remote_interfaces;
  shell::mojom::InterfaceProviderPtr local_interfaces;

  app_urls_.insert(url);

  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->set_target(
      Identity(url, std::string(), GetPermissiveCapabilityFilter()));
  params->set_remote_interfaces(GetProxy(&remote_interfaces));
  params->set_local_interfaces(std::move(local_interfaces));
  params->set_on_application_end(
      base::Bind(&Context::OnApplicationEnd, base::Unretained(this), url));
  application_manager_->ConnectToApplication(std::move(params));
}

void Context::RunCommandLineApplication(const base::Closure& callback) {
  DCHECK(app_urls_.empty());
  DCHECK(app_complete_callback_.is_null());
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line->GetArgs();
  for (size_t i = 0; i < args.size(); ++i) {
    GURL possible_app(args[i]);
    if (possible_app.SchemeIs("mojo")) {
      Run(possible_app);
      app_complete_callback_ = callback;
      break;
    }
  }
}

void Context::OnApplicationEnd(const GURL& url) {
  if (app_urls_.find(url) != app_urls_.end()) {
    app_urls_.erase(url);
    if (app_urls_.empty() && base::MessageLoop::current()->is_running()) {
      DCHECK_EQ(base::MessageLoop::current()->task_runner(),
                task_runners_->shell_runner());
      if (app_complete_callback_.is_null()) {
        base::MessageLoop::current()->QuitWhenIdle();
      } else {
        app_complete_callback_.Run();
      }
    }
  }
}

}  // namespace shell
}  // namespace mojo
