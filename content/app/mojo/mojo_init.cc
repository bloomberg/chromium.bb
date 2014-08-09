// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "base/logging.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/embedder/embedder.h"

namespace content {

void InitializeMojo() {
  mojo::embedder::Init();
  mojo::ApplicationManager::GetInstance();
}

}  // namespace content
