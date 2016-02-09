// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/standalone/android/android_handler_loader.h"

#include <utility>

namespace mojo {
namespace shell {

AndroidHandlerLoader::AndroidHandlerLoader() {}

AndroidHandlerLoader::~AndroidHandlerLoader() {}

void AndroidHandlerLoader::Load(const GURL& url,
                                InterfaceRequest<mojom::ShellClient> request) {
  DCHECK(request.is_pending());
  shell_client_.reset(
      new ShellConnection(&android_handler_, std::move(request)));
}

}  // namespace shell
}  // namespace mojo
