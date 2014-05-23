// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "base/logging.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/service_manager/service_manager.h"

namespace content {

namespace {

mojo::Environment* environment = NULL;

}  // namespace

void InitializeMojo() {
  DCHECK(!environment);
  environment = new mojo::Environment;
  mojo::embedder::Init();
  mojo::ServiceManager::GetInstance();
}

void ShutdownMojo() {
  delete environment;
  environment = NULL;
}

}  // namespace content
