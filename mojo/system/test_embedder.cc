// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/test_embedder.h"

#include "base/logging.h"
#include "mojo/system/core_impl.h"

namespace mojo {
namespace embedder {
namespace test {

void Shutdown() {
  system::CoreImpl* core_impl = static_cast<system::CoreImpl*>(Core::Get());
  CHECK(core_impl);
  Core::Reset();

  // TODO(vtl): Check for leaks, etc.

  delete core_impl;
}

}  // namespace test
}  // namespace embedder
}  // namespace mojo
