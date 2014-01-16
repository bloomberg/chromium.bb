// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/context.h"

#include "mojo/shell/dynamic_service_loader.h"
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
  dynamic_service_loader_.reset(new DynamicServiceLoader(this));
  service_manager_.set_default_loader(dynamic_service_loader_.get());
}

Context::~Context() {
  service_manager_.set_default_loader(NULL);
}

}  // namespace shell
}  // namespace mojo
