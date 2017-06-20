// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_request_handler.h"

namespace content {

mojom::URLLoaderFactoryPtr
URLLoaderRequestHandler::MaybeCreateSubresourceFactory() {
  return nullptr;
}

}  // namespace content