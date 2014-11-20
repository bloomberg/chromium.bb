// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "base/memory/scoped_ptr.h"
#include "mojo/edk/embedder/configuration.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/simple_platform_support.h"

namespace content {

void InitializeMojo() {
  // Things like content_shell and DevTools ocassionally send big
  // message which includes whole rendered screen or all loaded
  // scripts. The buffer size has to be big enough to allow such use
  // cases.
  mojo::embedder::GetConfiguration()->max_message_num_bytes = 64*1024*1024;
  mojo::embedder::Init(scoped_ptr<mojo::embedder::PlatformSupport>(
      new mojo::embedder::SimplePlatformSupport()));
}

}  // namespace content
