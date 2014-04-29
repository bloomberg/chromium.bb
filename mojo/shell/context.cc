// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/context.h"

#include "build/build_config.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "mojo/embedder/embedder.h"
#include "mojo/gles2/gles2_support_impl.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/services/native_viewport/native_viewport_service.h"
#include "mojo/shell/dynamic_service_loader.h"
#include "mojo/shell/in_process_dynamic_service_runner.h"
#include "mojo/shell/network_delegate.h"
#include "mojo/shell/out_of_process_dynamic_service_runner.h"
#include "mojo/shell/switches.h"
#include "mojo/spy/spy.h"

#if defined(OS_LINUX)
#include "mojo/shell/dbus_service_loader_linux.h"
#endif  // defined(OS_LINUX)

#if defined(USE_AURA)
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view_manager_connection.h"
#endif

namespace mojo {
namespace shell {
namespace {

// Used to ensure we only init once.
class Setup {
 public:
  Setup() {
    embedder::Init();
    gles2::GLES2SupportImpl::Init();
  }

  ~Setup() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

static base::LazyInstance<Setup> setup = LAZY_INSTANCE_INITIALIZER;

#if defined(USE_AURA)
class ViewManagerLoader : public ServiceLoader {
 public:
  ViewManagerLoader() {}
  virtual ~ViewManagerLoader() {}

 private:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedShellHandle shell_handle) OVERRIDE {
    scoped_ptr<Application> app(new Application(shell_handle.Pass()));
    app->AddServiceConnector(
        new ServiceConnector<services::view_manager::ViewManagerConnection,
                             services::view_manager::RootNodeManager>(
                                 &root_node_manager_));
    apps_.push_back(app.release());
  }

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
  }

  services::view_manager::RootNodeManager root_node_manager_;
  ScopedVector<Application> apps_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerLoader);
};
#endif

}  // namespace

class Context::NativeViewportServiceLoader : public ServiceLoader {
 public:
  explicit NativeViewportServiceLoader(Context* context) : context_(context) {}
  virtual ~NativeViewportServiceLoader() {}

 private:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedShellHandle service_handle) OVERRIDE {
    app_.reset(::CreateNativeViewportService(context_, service_handle.Pass()));
  }

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
  }

  Context* context_;
  scoped_ptr<Application> app_;
  DISALLOW_COPY_AND_ASSIGN(NativeViewportServiceLoader);
};

Context::Context()
    : task_runners_(base::MessageLoop::current()->message_loop_proxy()),
      storage_(),
      loader_(task_runners_.io_runner(),
              task_runners_.file_runner(),
              task_runners_.cache_runner(),
              scoped_ptr<net::NetworkDelegate>(new NetworkDelegate()),
              storage_.profile_path()) {
  setup.Get();
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  scoped_ptr<DynamicServiceRunnerFactory> runner_factory;
  if (cmdline->HasSwitch(switches::kEnableMultiprocess))
    runner_factory.reset(new OutOfProcessDynamicServiceRunnerFactory());
  else
    runner_factory.reset(new InProcessDynamicServiceRunnerFactory());

  service_manager_.set_default_loader(
      scoped_ptr<ServiceLoader>(
          new DynamicServiceLoader(this, runner_factory.Pass())));
  service_manager_.SetLoaderForURL(
      scoped_ptr<ServiceLoader>(new NativeViewportServiceLoader(this)),
      GURL("mojo:mojo_native_viewport_service"));
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

  if (cmdline->HasSwitch(switches::kSpy)) {
    spy_.reset(new mojo::Spy(&service_manager_,
                             cmdline->GetSwitchValueASCII(switches::kSpy)));
  }
}

Context::~Context() {
  service_manager_.set_default_loader(scoped_ptr<ServiceLoader>());
}

}  // namespace shell
}  // namespace mojo
