// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/context.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/devtools_service/public/cpp/switches.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/common/trace_controller_impl.h"
#include "mojo/common/tracing_impl.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/runner/about_fetcher.h"
#include "mojo/runner/in_process_native_runner.h"
#include "mojo/runner/out_of_process_native_runner.h"
#include "mojo/runner/switches.h"
#include "mojo/services/tracing/tracing.mojom.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/switches.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"

namespace mojo {
namespace runner {
namespace {

// Used to ensure we only init once.
class Setup {
 public:
  Setup() {
    embedder::Init(make_scoped_ptr(new embedder::SimplePlatformSupport()));
  }

  ~Setup() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

bool ConfigureURLMappings(const base::CommandLine& command_line,
                          Context* context) {
  URLResolver* resolver = context->url_resolver();

  // Configure the resolution of unknown mojo: URLs.
  GURL base_url;
  if (!command_line.HasSwitch(switches::kUseUpdater)) {
    // Use the shell's file root if the base was not specified.
    base_url = context->ResolveShellFileURL(std::string());
  }

  if (base_url.is_valid())
    resolver->SetMojoBaseURL(base_url);

  // The network service and updater must be loaded from the filesystem.
  // This mapping is done before the command line URL mapping are processed, so
  // that it can be overridden.
  resolver->AddURLMapping(GURL("mojo:network_service"),
                          context->ResolveShellFileURL(
                              "file:network_service/network_service.mojo"));
  resolver->AddURLMapping(
      GURL("mojo:updater"),
      context->ResolveShellFileURL("file:updater/updater.mojo"));

  // Command line URL mapping.
  std::vector<URLResolver::OriginMapping> origin_mappings =
      URLResolver::GetOriginMappings(command_line.argv());
  for (const auto& origin_mapping : origin_mappings)
    resolver->AddOriginMapping(GURL(origin_mapping.origin),
                               GURL(origin_mapping.base_url));

  if (command_line.HasSwitch(switches::kURLMappings)) {
    const std::string mappings =
        command_line.GetSwitchValueASCII(switches::kURLMappings);

    base::StringPairs pairs;
    if (!base::SplitStringIntoKeyValuePairs(mappings, '=', ',', &pairs))
      return false;
    using StringPair = std::pair<std::string, std::string>;
    for (const StringPair& pair : pairs) {
      const GURL from(pair.first);
      const GURL to = context->ResolveCommandLineURL(pair.second);
      if (!from.is_valid() || !to.is_valid())
        return false;
      resolver->AddURLMapping(from, to);
    }
  }

  return true;
}

void InitContentHandlers(shell::ApplicationManager* manager,
                         const base::CommandLine& command_line) {
  // Default content handlers.
  manager->RegisterContentHandler("application/pdf", GURL("mojo:pdf_viewer"));
  manager->RegisterContentHandler("image/png", GURL("mojo:png_viewer"));
  manager->RegisterContentHandler("text/html", GURL("mojo:html_viewer"));

  // Command-line-specified content handlers.
  std::string handlers_spec =
      command_line.GetSwitchValueASCII(switches::kContentHandlers);
  if (handlers_spec.empty())
    return;

#if defined(OS_ANDROID)
  // TODO(eseidel): On Android we pass command line arguments is via the
  // 'parameters' key on the intent, which we specify during 'am shell start'
  // via --esa, however that expects comma-separated values and says:
  //   am shell --help:
  //     [--esa <EXTRA_KEY> <EXTRA_STRING_VALUE>[,<EXTRA_STRING_VALUE...]]
  //     (to embed a comma into a string escape it using "\,")
  // Whatever takes 'parameters' and constructs a CommandLine is failing to
  // un-escape the commas, we need to move this fix to that file.
  base::ReplaceSubstringsAfterOffset(&handlers_spec, 0, "\\,", ",");
#endif

  std::vector<std::string> parts = base::SplitString(
      handlers_spec, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() % 2 != 0) {
    LOG(ERROR) << "Invalid value for switch " << switches::kContentHandlers
               << ": must be a comma-separated list of mimetype/url pairs."
               << handlers_spec;
    return;
  }

  for (size_t i = 0; i < parts.size(); i += 2) {
    GURL url(parts[i + 1]);
    if (!url.is_valid()) {
      LOG(ERROR) << "Invalid value for switch " << switches::kContentHandlers
                 << ": '" << parts[i + 1] << "' is not a valid URL.";
      return;
    }
    // TODO(eseidel): We should also validate that the mimetype is valid
    // net/base/mime_util.h could do this, but we don't want to depend on net.
    manager->RegisterContentHandler(parts[i], url);
  }
}

void InitNativeOptions(shell::ApplicationManager* manager,
                       const base::CommandLine& command_line) {
  std::vector<std::string> force_in_process_url_list = base::SplitString(
      command_line.GetSwitchValueASCII(switches::kForceInProcess), ",",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& force_in_process_url : force_in_process_url_list) {
    GURL gurl(force_in_process_url);
    if (!gurl.is_valid()) {
      LOG(ERROR) << "Invalid value for switch " << switches::kForceInProcess
                 << ": '" << force_in_process_url << "'is not a valid URL.";
      return;
    }

    shell::NativeRunnerFactory::Options options;
    options.force_in_process = true;
    manager->SetNativeOptionsForURL(options, gurl);
  }
}

void InitDevToolsServiceIfNeeded(shell::ApplicationManager* manager,
                                 const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(devtools_service::kRemoteDebuggingPort))
    return;

  std::string port_str =
      command_line.GetSwitchValueASCII(devtools_service::kRemoteDebuggingPort);
  unsigned port;
  if (!base::StringToUint(port_str, &port) || port > 65535) {
    LOG(ERROR) << "Invalid value for switch "
               << devtools_service::kRemoteDebuggingPort << ": '" << port_str
               << "' is not a valid port number.";
    return;
  }

  ServiceProviderPtr devtools_service_provider;
  URLRequestPtr request(URLRequest::New());
  request->url = "mojo:devtools_service";
  manager->ConnectToApplication(
      nullptr, request.Pass(), std::string(), GURL("mojo:shell"),
      GetProxy(&devtools_service_provider), nullptr,
      shell::GetPermissiveCapabilityFilter(), base::Closure());

  devtools_service::DevToolsCoordinatorPtr devtools_coordinator;
  devtools_service_provider->ConnectToService(
      devtools_service::DevToolsCoordinator::Name_,
      GetProxy(&devtools_coordinator).PassMessagePipe());
  devtools_coordinator->Initialize(static_cast<uint16_t>(port));
}

class TracingServiceProvider : public ServiceProvider {
 public:
  explicit TracingServiceProvider(InterfaceRequest<ServiceProvider> request)
      : binding_(this, request.Pass()) {}
  ~TracingServiceProvider() override {}

  void ConnectToService(const String& service_name,
                        ScopedMessagePipeHandle client_handle) override {
    if (service_name == tracing::TraceController::Name_) {
      new TraceControllerImpl(
          MakeRequest<tracing::TraceController>(client_handle.Pass()));
    }
  }

 private:
  StrongBinding<ServiceProvider> binding_;

  DISALLOW_COPY_AND_ASSIGN(TracingServiceProvider);
};

}  // namespace

Context::Context() : application_manager_(this) {
  DCHECK(!base::MessageLoop::current());

  // By default assume that the local apps reside alongside the shell.
  // TODO(ncbray): really, this should be passed in rather than defaulting.
  // This default makes sense for desktop but not Android.
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  SetShellFileRoot(shell_dir);

  base::FilePath cwd;
  PathService::Get(base::DIR_CURRENT, &cwd);
  SetCommandLineCWD(cwd);
}

Context::~Context() {
  DCHECK(!base::MessageLoop::current());
}

// static
void Context::EnsureEmbedderIsInitialized() {
  static base::LazyInstance<Setup>::Leaky setup = LAZY_INSTANCE_INITIALIZER;
  setup.Get();
}

void Context::SetShellFileRoot(const base::FilePath& path) {
  shell_file_root_ =
      util::AddTrailingSlashIfNeeded(util::FilePathToFileURL(path));
}

GURL Context::ResolveShellFileURL(const std::string& path) {
  return shell_file_root_.Resolve(path);
}

void Context::SetCommandLineCWD(const base::FilePath& path) {
  command_line_cwd_ =
      util::AddTrailingSlashIfNeeded(util::FilePathToFileURL(path));
}

GURL Context::ResolveCommandLineURL(const std::string& path) {
  return command_line_cwd_.Resolve(path);
}

bool Context::Init() {
  TRACE_EVENT0("mojo_shell", "Context::Init");
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  EnsureEmbedderIsInitialized();
  task_runners_.reset(
      new TaskRunners(base::MessageLoop::current()->task_runner()));

  // TODO(vtl): Probably these failures should be checked before |Init()|, and
  // this function simply shouldn't fail.
  if (!shell_file_root_.is_valid())
    return false;
  if (!ConfigureURLMappings(command_line, this))
    return false;

  // TODO(vtl): This should be MASTER, not NONE.
  embedder::InitIPCSupport(
      embedder::ProcessType::NONE, task_runners_->shell_runner(), this,
      task_runners_->io_runner(), embedder::ScopedPlatformHandle());

  scoped_ptr<shell::NativeRunnerFactory> runner_factory;
  if (command_line.HasSwitch(switches::kEnableMultiprocess))
    runner_factory.reset(new OutOfProcessNativeRunnerFactory(this));
  else
    runner_factory.reset(new InProcessNativeRunnerFactory(this));
  application_manager_.set_blocking_pool(task_runners_->blocking_pool());
  application_manager_.set_native_runner_factory(runner_factory.Pass());
  application_manager_.set_disable_cache(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCache));

  InitContentHandlers(&application_manager_, command_line);
  InitNativeOptions(&application_manager_, command_line);

  ServiceProviderPtr tracing_service_provider_ptr;
  new TracingServiceProvider(GetProxy(&tracing_service_provider_ptr));
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:tracing");
  application_manager_.ConnectToApplication(
      nullptr, request.Pass(), std::string(), GURL(""), nullptr,
      tracing_service_provider_ptr.Pass(),
      shell::GetPermissiveCapabilityFilter(), base::Closure());

  InitDevToolsServiceIfNeeded(&application_manager_, command_line);

  return true;
}

void Context::Shutdown() {
  TRACE_EVENT0("mojo_shell", "Context::Shutdown");
  DCHECK_EQ(base::MessageLoop::current()->task_runner(),
            task_runners_->shell_runner());
  embedder::ShutdownIPCSupport();
  // We'll quit when we get OnShutdownComplete().
  base::MessageLoop::current()->Run();
}

GURL Context::ResolveMappings(const GURL& url) {
  return url_resolver_.ApplyMappings(url);
}

GURL Context::ResolveMojoURL(const GURL& url) {
  return url_resolver_.ResolveMojoURL(url);
}

bool Context::CreateFetcher(
    const GURL& url,
    const shell::Fetcher::FetchCallback& loader_callback) {
  if (url.SchemeIs(AboutFetcher::kAboutScheme)) {
    AboutFetcher::Start(url, loader_callback);
    return true;
  }

  return false;
}

void Context::OnShutdownComplete() {
  DCHECK_EQ(base::MessageLoop::current()->task_runner(),
            task_runners_->shell_runner());
  base::MessageLoop::current()->Quit();
}

void Context::Run(const GURL& url) {
  DCHECK(app_complete_callback_.is_null());
  ServiceProviderPtr services;
  ServiceProviderPtr exposed_services;

  app_urls_.insert(url);
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(url.spec());
  application_manager_.ConnectToApplication(
      nullptr, request.Pass(), std::string(), GURL(), GetProxy(&services),
      exposed_services.Pass(), shell::GetPermissiveCapabilityFilter(),
      base::Bind(&Context::OnApplicationEnd, base::Unretained(this), url));
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
        base::MessageLoop::current()->Quit();
      } else {
        app_complete_callback_.Run();
      }
    }
  }
}

}  // namespace runner
}  // namespace mojo
