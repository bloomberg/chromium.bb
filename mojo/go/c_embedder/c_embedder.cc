// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/go/c_embedder/c_embedder.h"

#include "base/memory/scoped_ptr.h"
#include "mojo/embedder/embedder.h"
#include "mojo/embedder/simple_platform_support.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitializeMojoEmbedder() {
  mojo::embedder::Init(scoped_ptr<mojo::embedder::PlatformSupport>(
      new mojo::embedder::SimplePlatformSupport()));
}

#ifdef __cplusplus
}  // extern "C"
#endif
