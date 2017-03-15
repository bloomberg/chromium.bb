// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/watcher.h"

#include "mojo/public/c/system/functions.h"

namespace mojo {

MojoResult CreateWatcher(MojoWatcherCallback callback,
                         ScopedWatcherHandle* watcher_handle) {
  MojoHandle handle;
  MojoResult rv = MojoCreateWatcher(callback, &handle);
  if (rv == MOJO_RESULT_OK)
    watcher_handle->reset(WatcherHandle(handle));
  return rv;
}

}  // namespace mojo
