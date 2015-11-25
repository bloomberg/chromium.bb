// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_BROKER_H_
#define MOJO_EDK_SYSTEM_BROKER_H_

#include <stdint.h>
#include <vector>

#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace mojo {
namespace edk {

// An interface for communicating to a central "broker" process from each
// process using the EDK. This is needed because child processes are sandboxed.
// It is safe to call from any thread.
class MOJO_SYSTEM_IMPL_EXPORT Broker {
 public:
  virtual ~Broker() {}

#if defined(OS_WIN)
  // All these methods are needed because sandboxed Windows processes can't
  // create named pipes or duplicate handles.

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
#endif
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_BROKER_H_
