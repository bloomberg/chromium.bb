// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/current_thread_loader.h"

namespace content {

CurrentThreadLoader::CurrentThreadLoader(const ApplicationFactory& factory)
    : factory_(factory) {}

CurrentThreadLoader::~CurrentThreadLoader() {}

void CurrentThreadLoader::Load(const std::string& name,
                               mojo::shell::mojom::ShellClientRequest request) {
  if (!shell_client_) {
    shell_client_ = factory_.Run();
    factory_ = ApplicationFactory();
  }

  connections_.push_back(make_scoped_ptr(
      new mojo::ShellConnection(shell_client_.get(), std::move(request))));
}

}  // namespace content
