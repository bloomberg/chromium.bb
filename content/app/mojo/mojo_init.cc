// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "base/memory/scoped_ptr.h"
#include "mojo/embedder/embedder.h"
#include "mojo/embedder/simple_platform_support.h"

namespace content {

void InitializeMojo() {
  mojo::embedder::Init(scoped_ptr<mojo::embedder::PlatformSupport>(
      new mojo::embedder::SimplePlatformSupport()));
}

}  // namespace content
