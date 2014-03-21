// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "base/lazy_instance.h"
#include "mojo/embedder/embedder.h"
#include "mojo/service_manager/service_manager.h"

namespace content {

namespace {

struct Initializer {
  Initializer() {
    // TODO(davemoore): Configuration goes here. For now just create the shared
    // instance of the ServiceManager.
    mojo::ServiceManager::GetInstance();
    mojo::embedder::Init();
  }
};

static base::LazyInstance<Initializer>::Leaky initializer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Initializes mojo. Use a lazy instance to ensure we only do this once.
void InitializeMojo() {
  initializer.Get();
}

}  // namespace content
