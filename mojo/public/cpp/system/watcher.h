// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_WATCHER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_WATCHER_H_

#include "mojo/public/c/system/types.h"
#include "mojo/public/c/system/watcher.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A strongly-typed representation of a |MojoHandle| for a watcher.
class WatcherHandle : public Handle {
 public:
  WatcherHandle() = default;
  explicit WatcherHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

static_assert(sizeof(WatcherHandle) == sizeof(Handle),
              "Bad size for C++ WatcherHandle");

typedef ScopedHandleBase<WatcherHandle> ScopedWatcherHandle;
static_assert(sizeof(ScopedWatcherHandle) == sizeof(WatcherHandle),
              "Bad size for C++ ScopedWatcherHandle");

MOJO_CPP_SYSTEM_EXPORT MojoResult
CreateWatcher(MojoWatcherCallback callback,
              ScopedWatcherHandle* watcher_handle);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_WATCHER_H_
