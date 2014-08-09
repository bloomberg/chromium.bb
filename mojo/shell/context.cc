// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/context.h"

#include <vector>

#include "build/build_config.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_split.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/service_manager/background_shell_service_loader.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/services/native_viewport/native_viewport_service.h"
#include "mojo/shell/dynamic_service_loader.h"
#include "mojo/shell/in_process_dynamic_service_runner.h"
#include "mojo/shell/out_of_process_dynamic_service_runner.h"
#include "mojo/shell/switches.h"
#include "mojo/shell/ui_service_loader_android.h"
#include "mojo/spy/spy.h"

#if defined(OS_LINUX)
#include "mojo/shell/dbus_service_loader_linux.h"
#endif  // defined(OS_LINUX)

#if defined(OS_ANDROID)
#include "mojo/shell/network_service_loader.h"
#endif  // defined(OS_ANDROID)

#if defined(USE_AURA)
#include "mojo/shell/view_manager_loader.h"
#endif

namespace mojo {
namespace shell {
namespace {

// These mojo: URLs are loaded directly from the local filesystem. They
// correspond to shared libraries bundled alongside the mojo_shell.
const char* kLocalMojoURLs[] = {
  "mojo:mojo_network_service",
};

// Used to ensure we only init once.
class Setup {
 public:
  Setup() {
    embedder::Init();
  }

  ~Setup() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

static base::LazyInstance<Setup>::Leaky setup = LAZY_INSTANCE_INITIALIZER;

void InitContentHandlers(DynamicServiceLoader* loader,
                         base::CommandLine* command_line) {
  std::string handlers_spec = command_line->GetSwitchValueASCII(
      switches::kContentHandlers);
  if (handlers_spec.empty())
    return;

  std::vector<std::string> parts;
  base::SplitString(handlers_spec, ',', &parts);
  if (parts.size() % 2 != 0) {
    LOG(ERROR) << "Invalid value for switch " << switches::kContentHandlers
               << ": must be a comma-separated list of mimetype/url pairs.";
    return;
  }

  for (size_t i = 0; i < parts.size(); i += 2) {
    GURL url(parts[i + 1]);
    if (!url.is_valid()) {
      LOG(ERROR) << "Invalid value for switch " << switches::kContentHandlers
                 << ": '" << parts[i + 1] << "' is not a valid URL.";
      return;
    }
    loader->RegisterContentHandler(parts[i], url);
  }
}

}  // namespace

class Context::NativeViewportServiceLoader : public ServiceLoader {
 public:
  NativeViewportServiceLoader() {}
  virtual ~NativeViewportServiceLoader() {}

 private:
  virtual void Load(ServiceManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE {
    ScopedMessagePipeHandle shell_handle = callbacks->RegisterApplication();
    if (shell_handle.is_valid())
      app_.reset(services::CreateNativeViewportService(shell_handle.Pass()));
  }

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
  }

  scoped_ptr<ApplicationImpl> app_;
  DISALLOW_COPY_AND_ASSIGN(NativeViewportServiceLoader);
};

Context::Context() {
  DCHECK(!base::MessageLoop::current());
}

void Context::Init() {
  setup.Get();
  task_runners_.reset(
      new TaskRunners(base::MessageLoop::current()->message_loop_proxy()));

  for (size_t i = 0; i < arraysize(kLocalMojoURLs); ++i)
    mojo_url_resolver_.AddLocalFileMapping(GURL(kLocalMojoURLs[i]));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  scoped_ptr<DynamicServiceRunnerFactory> runner_factory;
  if (command_line->HasSwitch(switches::kEnableMultiprocess))
    runner_factory.reset(new OutOfProcessDynamicServiceRunnerFactory());
  else
    runner_factory.reset(new InProcessDynamicServiceRunnerFactory());

  DynamicServiceLoader* dynamic_service_loader =
      new DynamicServiceLoader(this, runner_factory.Pass());
  InitContentHandlers(dynamic_service_loader, command_line);
  service_manager_.set_default_loader(
      scoped_ptr<ServiceLoader>(dynamic_service_loader));

  // The native viewport service synchronously waits for certain messages. If we
  // don't run it on its own thread we can easily deadlock. Long term native
  // viewport should run its own process so that this isn't an issue.
#if defined(OS_ANDROID)
  service_manager_.SetLoaderForURL(
      scoped_ptr<ServiceLoader>(
          new UIServiceLoader(
              scoped_ptr<ServiceLoader>(new NativeViewportServiceLoader()),
              this)),
      GURL("mojo:mojo_native_viewport_service"));
#else
  {
    scoped_ptr<BackgroundShellServiceLoader> loader(
        new BackgroundShellServiceLoader(
            scoped_ptr<ServiceLoader>(new NativeViewportServiceLoader()),
            "native_viewport",
             base::MessageLoop::TYPE_UI));
    service_manager_.SetLoaderForURL(
        loader.PassAs<ServiceLoader>(),
        GURL("mojo:mojo_native_viewport_service"));
  }
#endif
#if defined(USE_AURA)
  // TODO(sky): need a better way to find this. It shouldn't be linked in.
  service_manager_.SetLoaderForURL(
      scoped_ptr<ServiceLoader>(new ViewManagerLoader()),
      GURL("mojo:mojo_view_manager"));
#endif

#if defined(OS_LINUX)
  service_manager_.SetLoaderForScheme(
      scoped_ptr<ServiceLoader>(new DBusServiceLoader(this)),
      "dbus");
#endif  // defined(OS_LINUX)

  if (command_line->HasSwitch(switches::kSpy)) {
    spy_.reset(new mojo::Spy(
        &service_manager_, command_line->GetSwitchValueASCII(switches::kSpy)));
  }

#if defined(OS_ANDROID)
  // On android, the network service is bundled with the shell because the
  // network stack depends on the android runtime.
  {
    scoped_ptr<BackgroundShellServiceLoader> loader(
        new BackgroundShellServiceLoader(
            scoped_ptr<ServiceLoader>(new NetworkServiceLoader()),
            "network_service",
             base::MessageLoop::TYPE_IO));
    service_manager_.SetLoaderForURL(loader.PassAs<ServiceLoader>(),
                                     GURL("mojo:mojo_network_service"));
  }
#endif
}

Context::~Context() {
  DCHECK(!base::MessageLoop::current());
}

}  // namespace shell
}  // namespace mojo
