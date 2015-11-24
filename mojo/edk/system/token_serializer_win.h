// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_TOKEN_SERIALIZER_WIN_H_
#define MOJO_EDK_SYSTEM_TOKEN_SERIALIZER_WIN_H_

#include <stdint.h>
#include <vector>

#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace mojo {
namespace edk {

// An interface for serializing a Mojo handle to memory. A child process will
// have to make sync calls to the parent process. It is safe to call from any
// thread.
class MOJO_SYSTEM_IMPL_EXPORT TokenSerializer {
 public:
  virtual ~TokenSerializer() {}

  // Create a PlatformChannelPair.
  virtual void CreatePlatformChannelPair(ScopedPlatformHandle* server,
                                         ScopedPlatformHandle* client) = 0;

  // Converts the given platform handles to tokens.
  // |tokens| should point to memory that is sizeof(uint64_t) * count;
  virtual void HandleToToken(const PlatformHandle* platform_handles,
                             size_t count,
                             uint64_t* tokens) = 0;

  // Converts the given tokens to platformhandles.
  // |handles| should point to memory that is sizeof(HANDLE) * count;
  virtual void TokenToHandle(const uint64_t* tokens,
                             size_t count,
                             PlatformHandle* handles) = 0;
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_TOKEN_SERIALIZER_WIN_H_
