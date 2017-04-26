// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/embedder/process_type.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"

namespace base {
class Value;
}

namespace content {

class ContentBrowserClient;
class ContentGpuClient;
class ContentRendererClient;
class ContentUtilityClient;
class ZygoteForkDelegate;
struct MainFunctionParams;

class CONTENT_EXPORT ContentMainDelegate {
 public:
  virtual ~ContentMainDelegate() {}

  // Tells the embedder that the absolute basic startup has been done, i.e.
  // it's now safe to create singletons and check the command line. Return true
  // if the process should exit afterwards, and if so, |exit_code| should be
  // set. This is the place for embedder to do the things that must happen at
  // the start. Most of its startup code should be in the methods below.
  virtual bool BasicStartupComplete(int* exit_code);

  // This is where the embedder puts all of its startup code that needs to run
  // before the sandbox is engaged.
  virtual void PreSandboxStartup() {}

  // This is where the embedder can add startup code to run after the sandbox
  // has been initialized.
  virtual void SandboxInitialized(const std::string& process_type) {}

  // Asks the embedder to start a process. Return -1 for the default behavior.
  virtual int RunProcess(
      const std::string& process_type,
      const MainFunctionParams& main_function_params);

  // Called right before the process exits.
  virtual void ProcessExiting(const std::string& process_type) {}

#if defined(OS_MACOSX)
  // Returns true if the process registers with the system monitor, so that we
  // can allocate an IO port for it before the sandbox is initialized. Embedders
  // are called only for process types that content doesn't know about.
  virtual bool ProcessRegistersWithSystemProcess(
      const std::string& process_type);

  // Used to determine if we should send the mach port to the parent process or
  // not. The embedder usually sends it for all child processes, use this to
  // override this behavior.
  virtual bool ShouldSendMachPort(const std::string& process_type);

  // Allows the embedder to override initializing the sandbox. This is needed
  // because some processes might not want to enable it right away or might not
  // want it at all.
  virtual bool DelaySandboxInitialization(const std::string& process_type);

#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  // Tells the embedder that the zygote process is starting, and allows it to
  // specify one or more zygote delegates if it wishes by storing them in
  // |*delegates|.
  virtual void ZygoteStarting(
      std::vector<std::unique_ptr<ZygoteForkDelegate>>* delegates);

  // Called every time the zygote process forks.
  virtual void ZygoteForked() {}
#endif  // OS_MACOSX

  // TODO(vadimt, yiyaoliu): Remove this function once crbug.com/453640 is
  // fixed.
  // Returns whether or not profiler recording should be enabled.
  virtual bool ShouldEnableProfilerRecording();

  // Overrides the Service Manager process type to use for the currently running
  // process.
  virtual service_manager::ProcessType OverrideProcessType();

  // Creates a service catalog for the Service Manager to use when embedded by
  // content.
  virtual std::unique_ptr<base::Value> CreateServiceCatalog();

  // Allows the content embedder to adjust arbitrary command line arguments for
  // any service process started by the Service Manager.
  virtual void AdjustServiceProcessCommandLine(
      const service_manager::Identity& identity,
      base::CommandLine* command_line);

  // Indicates if the Service Manager should be terminated in response to a
  // specific service instance quitting. If this returns |true|, the value in
  // |*exit_code| will be returned from the Service Manager's process on exit.
  virtual bool ShouldTerminateServiceManagerOnInstanceQuit(
      const service_manager::Identity& identity,
      int* exit_code);

  // Allows the embedder to perform arbitrary initialization within the Service
  // Manager process immediately before the Service Manager runs its main loop.
  //
  // |quit_closure| is a callback the embedder may retain and invoke at any time
  // to cleanly terminate Service Manager execution.
  virtual void OnServiceManagerInitialized(
      const base::Closure& quit_closure,
      service_manager::BackgroundServiceManager* service_manager);

  // Allows the embedder to instantiate one of its own embedded services by
  // name. If the named service is unknown, this should return null.
  virtual std::unique_ptr<service_manager::Service> CreateEmbeddedService(
      const std::string& service_name);

 protected:
  friend class ContentClientInitializer;

  // Called once per relevant process type to allow the embedder to customize
  // content. If an embedder wants the default (empty) implementation, don't
  // override this.
  virtual ContentBrowserClient* CreateContentBrowserClient();
  virtual ContentGpuClient* CreateContentGpuClient();
  virtual ContentRendererClient* CreateContentRendererClient();
  virtual ContentUtilityClient* CreateContentUtilityClient();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_
