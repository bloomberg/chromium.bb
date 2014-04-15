// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/context.h"

#include "base/command_line.h"
#include "mojo/embedder/embedder.h"
#include "mojo/gles2/gles2_support_impl.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/services/native_viewport/native_viewport_service.h"
#include "mojo/shell/dynamic_service_loader.h"
#include "mojo/shell/in_process_dynamic_service_runner.h"
#include "mojo/shell/network_delegate.h"
#include "mojo/shell/out_of_process_dynamic_service_runner.h"
#include "mojo/shell/switches.h"
#include "mojo/spy/spy.h"

namespace mojo {
namespace shell {

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
  embedder::Init();
  gles2::GLES2SupportImpl::Init();

  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  scoped_ptr<DynamicServiceRunnerFactory> runner_factory;
  if (cmdline->HasSwitch(switches::kEnableMultiprocess))
    runner_factory.reset(new OutOfProcessDynamicServiceRunnerFactory());
  else
    runner_factory.reset(new InProcessDynamicServiceRunnerFactory());

  dynamic_service_loader_.reset(
      new DynamicServiceLoader(this, runner_factory.Pass()));
  service_manager_.set_default_loader(dynamic_service_loader_.get());
  native_viewport_service_loader_.reset(new NativeViewportServiceLoader(this));
  service_manager_.SetLoaderForURL(native_viewport_service_loader_.get(),
                                    GURL("mojo:mojo_native_viewport_service"));

  if (cmdline->HasSwitch(switches::kSpy)) {
    spy_.reset(new mojo::Spy(&service_manager_,
                             cmdline->GetSwitchValueASCII(switches::kSpy)));
  }
}

Context::~Context() {
  service_manager_.set_default_loader(NULL);
}

}  // namespace shell
}  // namespace mojo
