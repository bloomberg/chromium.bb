// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/standalone/android/android_handler_loader.h"

#include <utility>

namespace mojo {
namespace shell {

AndroidHandlerLoader::AndroidHandlerLoader() {}

AndroidHandlerLoader::~AndroidHandlerLoader() {}

void AndroidHandlerLoader::Load(
    const GURL& url,
    InterfaceRequest<mojom::Application> application_request) {
  DCHECK(application_request.is_pending());
  application_.reset(
      new ApplicationImpl(&android_handler_, std::move(application_request)));
}

}  // namespace shell
}  // namespace mojo
