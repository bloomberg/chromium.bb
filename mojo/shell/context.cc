// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/context.h"

#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "mojo/application_manager/application_loader.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/application_manager/background_shell_application_loader.h"
#include "mojo/embedder/embedder.h"
#include "mojo/embedder/simple_platform_support.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/native_viewport/gpu_impl.h"
#include "mojo/services/native_viewport/native_viewport_impl.h"
#include "mojo/shell/dynamic_application_loader.h"
#include "mojo/shell/in_process_dynamic_service_runner.h"
#include "mojo/shell/out_of_process_dynamic_service_runner.h"
#include "mojo/shell/switches.h"
#include "mojo/shell/ui_application_loader_android.h"
#include "mojo/spy/spy.h"
#include "ui/gl/gl_share_group.h"

#if defined(OS_LINUX)
#include "mojo/shell/dbus_application_loader_linux.h"
#endif  // defined(OS_LINUX)

#if defined(OS_ANDROID)
#include "mojo/shell/network_application_loader.h"
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
    embedder::Init(scoped_ptr<mojo::embedder::PlatformSupport>(
        new mojo::embedder::SimplePlatformSupport()));
  }

  ~Setup() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

static base::LazyInstance<Setup>::Leaky setup = LAZY_INSTANCE_INITIALIZER;

void InitContentHandlers(DynamicApplicationLoader* loader,
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

class Context::NativeViewportApplicationLoader
    : public ApplicationLoader,
      public ApplicationDelegate,
      public InterfaceFactory<NativeViewport>,
      public InterfaceFactory<Gpu> {
 public:
  NativeViewportApplicationLoader()
      : share_group_(new gfx::GLShareGroup),
        mailbox_manager_(new gpu::gles2::MailboxManager) {}
  virtual ~NativeViewportApplicationLoader() {}

 private:
  // ApplicationLoader implementation.
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE {
    ScopedMessagePipeHandle shell_handle = callbacks->RegisterApplication();
    if (shell_handle.is_valid())
      app_.reset(new ApplicationImpl(this, shell_handle.Pass()));
  }

  virtual void OnServiceError(ApplicationManager* manager,
                              const GURL& url) OVERRIDE {}

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) OVERRIDE {
    connection->AddService<NativeViewport>(this);
    connection->AddService<Gpu>(this);
    return true;
  }

  // InterfaceFactory<NativeViewport> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<NativeViewport> request) OVERRIDE {
    BindToRequest(new NativeViewportImpl, &request);
  }

  // InterfaceFactory<Gpu> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<Gpu> request) OVERRIDE {
    BindToRequest(new GpuImpl(share_group_.get(), mailbox_manager_.get()),
                  &request);
  }

  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  scoped_ptr<ApplicationImpl> app_;
  DISALLOW_COPY_AND_ASSIGN(NativeViewportApplicationLoader);
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

  DynamicApplicationLoader* dynamic_application_loader =
      new DynamicApplicationLoader(this, runner_factory.Pass());
  InitContentHandlers(dynamic_application_loader, command_line);
  application_manager_.set_default_loader(
      scoped_ptr<ApplicationLoader>(dynamic_application_loader));

  // The native viewport service synchronously waits for certain messages. If we
  // don't run it on its own thread we can easily deadlock. Long term native
  // viewport should run its own process so that this isn't an issue.
#if defined(OS_ANDROID)
  application_manager_.SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(new UIApplicationLoader(
          scoped_ptr<ApplicationLoader>(new NativeViewportApplicationLoader()),
          this)),
      GURL("mojo:mojo_native_viewport_service"));
#else
  {
    scoped_ptr<BackgroundShellApplicationLoader> loader(
        new BackgroundShellApplicationLoader(
            scoped_ptr<ApplicationLoader>(
                new NativeViewportApplicationLoader()),
            "native_viewport",
            base::MessageLoop::TYPE_UI));
    application_manager_.SetLoaderForURL(
        loader.PassAs<ApplicationLoader>(),
        GURL("mojo:mojo_native_viewport_service"));
  }
#endif
#if defined(USE_AURA)
  // TODO(sky): need a better way to find this. It shouldn't be linked in.
  application_manager_.SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(new ViewManagerLoader()),
      GURL("mojo:mojo_view_manager"));
#endif

#if defined(OS_LINUX)
  application_manager_.SetLoaderForScheme(
      scoped_ptr<ApplicationLoader>(new DBusApplicationLoader(this)), "dbus");
#endif  // defined(OS_LINUX)

  if (command_line->HasSwitch(switches::kSpy)) {
    spy_.reset(
        new mojo::Spy(&application_manager_,
                      command_line->GetSwitchValueASCII(switches::kSpy)));
  }

#if defined(OS_ANDROID)
  // On android, the network service is bundled with the shell because the
  // network stack depends on the android runtime.
  {
    scoped_ptr<BackgroundShellApplicationLoader> loader(
        new BackgroundShellApplicationLoader(
            scoped_ptr<ApplicationLoader>(new NetworkApplicationLoader()),
            "network_service",
            base::MessageLoop::TYPE_IO));
    application_manager_.SetLoaderForURL(loader.PassAs<ApplicationLoader>(),
                                         GURL("mojo:mojo_network_service"));
  }
#endif
}

Context::~Context() {
  DCHECK(!base::MessageLoop::current());
}

}  // namespace shell
}  // namespace mojo
