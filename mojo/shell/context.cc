// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/context.h"

#include "mojo/gles2/gles2_support_impl.h"
#include "mojo/shell/dynamic_service_loader.h"
#include "mojo/shell/in_process_dynamic_service_runner.h"
#include "mojo/shell/network_delegate.h"
#include "mojo/system/embedder/embedder.h"

namespace mojo {
namespace shell {

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

  scoped_ptr<DynamicServiceRunnerFactory> runner_factory(
      new InProcessDynamicServiceRunnerFactory());
  dynamic_service_loader_.reset(
      new DynamicServiceLoader(this, runner_factory.Pass()));
  service_manager_.set_default_loader(dynamic_service_loader_.get());
}

Context::~Context() {
  service_manager_.set_default_loader(NULL);
}

}  // namespace shell
}  // namespace mojo
