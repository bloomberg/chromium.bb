// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

namespace content {

namespace {

class MojoInitializer {
 public:
  MojoInitializer() {
    mojo::embedder::SetMaxMessageSize(IPC::Channel::kMaximumMessageSize);
    mojo::embedder::Init();
  }
};

base::LazyInstance<MojoInitializer>::Leaky mojo_initializer;

}  //  namespace

void InitializeMojo() {
  mojo_initializer.Get();
}

}  // namespace content
