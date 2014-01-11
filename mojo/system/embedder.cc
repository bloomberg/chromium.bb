// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/embedder.h"

#include "mojo/system/core_impl.h"

namespace mojo {
namespace embedder {

void Init() {
  Core::Init(new system::CoreImpl());
}

}  // namespace embedder
}  // namespace mojo
