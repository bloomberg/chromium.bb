// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/platform_handle_vector.h"

namespace mojo {
namespace embedder {

void CloseAllHandlesAndClear(PlatformHandleVector* platform_handles) {
  for (PlatformHandleVector::iterator it = platform_handles->begin();
       it != platform_handles->end();
       ++it)
    it->CloseIfNecessary();
  platform_handles->clear();
}

}  // namespace embedder
}  // namespace mojo
