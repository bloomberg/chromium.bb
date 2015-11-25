// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_SIMPLE_BROKER_H_
#define MOJO_EDK_SYSTEM_SIMPLE_BROKER_H_

#include "mojo/edk/system/broker.h"

namespace mojo {
namespace edk {

// A simple implementation of Broker interface. This isn't meant for production
// use (i.e. with multi-process, sandboxes). It's provided for use by unittests.
// Implementation note: this default implementation works across processes
// without a sandbox.
class MOJO_SYSTEM_IMPL_EXPORT SimpleBroker : public Broker {
 public:
  SimpleBroker();
  ~SimpleBroker() override;

  // Broker implementation:
#if defined(OS_WIN)
  void CreatePlatformChannelPair(ScopedPlatformHandle* server,
                                 ScopedPlatformHandle* client) override;
  void HandleToToken(const PlatformHandle* platform_handles,
                     size_t count,
                     uint64_t* tokens) override;
  void TokenToHandle(const uint64_t* tokens,
                     size_t count,
                     PlatformHandle* handles) override;
#endif
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_SIMPLE_BROKER_H_
